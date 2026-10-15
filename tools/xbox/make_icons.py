#!/usr/bin/env python3
"""The Xbox dashboard's two pictures, made from the game's own art.

The dashboard shows a title image (128x128) beside the game and a save
image (64x64) beside its save. The N64 game has neither. The Dreamcast port
gives its VMU save a 32x32 icon of Fox's face (sf64.ico); the Xbox makes
both pictures out of the same face at full quality -- Fox's radio portrait
(aFoxPortraitTex, 44x44 RGBA16 in the common segment), read from the PNG
Torch exports from the user's ROM. Nothing ROM-derived is committed: the
BMPs land in Platform/xbox/gen/ (gitignored), and RXDK's bundler turns them
into the DXT1 .xbx the dashboard needs (Platform/xbox/titleimage.rdf,
saveimage.rdf).

The portrait's one-pixel orange radio frame is cropped away (the dashboard
draws its own frame), and the 42x42 face is scaled up with a Catmull-Rom
filter.

Usage: python tools/xbox/make_icons.py   (after Torch has exported src/assets)
"""
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCE = os.path.join(ROOT, "src", "assets", "ast_common", "aFoxPortraitTex.rgba16.png")
OUT = os.path.join(ROOT, "Platform", "xbox", "gen")
FRAME = 1                         # the radio frame, cropped on every side


def read_png(path):
    """8-bit RGBA, non-interlaced: what Torch writes. Returns (w, h, rows of (r,g,b,a))."""
    with open(path, "rb") as f:
        d = f.read()
    if d[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("make_icons: %s is not a PNG" % path)
    p, idat, hdr = 8, b"", None
    while p < len(d):
        n, t = struct.unpack(">I4s", d[p:p + 8])
        c = d[p + 8:p + 8 + n]
        p += 12 + n
        if t == b"IHDR":
            hdr = struct.unpack(">IIBBBBB", c)
        elif t == b"IDAT":
            idat += c
    w, h, depth, ctype, _, _, interlace = hdr
    if depth != 8 or ctype != 6 or interlace != 0:
        raise SystemExit("make_icons: %s is not 8-bit RGBA non-interlaced" % path)
    raw = zlib.decompress(idat)
    st, bpp = w * 4, 4
    rows, prev, i = [], bytearray(st), 0
    for _ in range(h):
        flt = raw[i]
        line = bytearray(raw[i + 1:i + 1 + st])
        i += 1 + st
        for x in range(st):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if flt == 1:
                line[x] = (line[x] + a) & 255
            elif flt == 2:
                line[x] = (line[x] + b) & 255
            elif flt == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif flt == 4:
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
            elif flt != 0:
                raise SystemExit("make_icons: bad PNG filter %d" % flt)
        rows.append([tuple(line[x * 4:x * 4 + 4]) for x in range(w)])
        prev = line
    return w, h, rows


def catmull_rom(t):
    t = abs(t)
    if t < 1:
        return 1.5 * t ** 3 - 2.5 * t ** 2 + 1
    if t < 2:
        return -0.5 * t ** 3 + 2.5 * t ** 2 - 4 * t + 2
    return 0.0


def resample(rows, size):
    """Separable Catmull-Rom from a square picture to size x size (RGB floats 0..1)."""
    n = len(rows)
    scale = n / float(size)

    def weights(o):
        c = (o + 0.5) * scale - 0.5
        base = int(c // 1)
        ws = [(min(max(base + k, 0), n - 1), catmull_rom(c - (base + k))) for k in (-1, 0, 1, 2)]
        tot = sum(w for _, w in ws)
        return [(i, w / tot) for i, w in ws]

    wx = [weights(o) for o in range(size)]
    tmp = [[tuple(sum(rows[y][i][ch] * w for i, w in wx[x]) for ch in range(3))
            for x in range(size)] for y in range(n)]
    out = []
    for y in range(size):
        wy = weights(y)
        out.append([tuple(min(255.0, max(0.0, sum(tmp[i][x][ch] * w for i, w in wy))) / 255.0
                          for ch in range(3)) for x in range(size)])
    return out


def write_bmp(path, img):
    """24-bit bottom-up BMP: what the RXDK bundler reads."""
    h, w = len(img), len(img[0])
    row = (w * 3 + 3) & ~3
    data = bytearray()
    for y in range(h - 1, -1, -1):
        line = bytearray()
        for r, g, b in img[y]:
            line += bytes((int(b * 255 + 0.5), int(g * 255 + 0.5), int(r * 255 + 0.5)))
        line += b"\0" * (row - len(line))
        data += line
    hdr = struct.pack("<2sIHHI", b"BM", 54 + len(data), 0, 0, 54)
    dib = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(hdr + dib + data)
    os.replace(tmp, path)


def main():
    if not os.path.isfile(SOURCE):
        raise SystemExit("make_icons: %s is missing -- run Torch first (setup.py)" % SOURCE)
    w, h, rows = read_png(SOURCE)
    if w != 44 or h != 44:
        raise SystemExit("make_icons: aFoxPortraitTex is %dx%d, expected 44x44" % (w, h))
    face = [r[FRAME:w - FRAME] for r in rows[FRAME:h - FRAME]]
    os.makedirs(OUT, exist_ok=True)
    for name, size in (("titleimage", 128), ("saveimage", 64)):
        write_bmp(os.path.join(OUT, name + ".bmp"), resample(face, size))
    print("make_icons: titleimage.bmp 128x128, saveimage.bmp 64x64 -> Platform/xbox/gen")


if __name__ == "__main__":
    main()
