#!/usr/bin/env python3
"""The Xbox pad's button art, from the Dreamcast port's (src/dc/).

    python tools/xbox/make_xbox_buttons.py

The Dreamcast port drew its own on-screen buttons for its pad (src/dc/, CC0
with the rest of it): the versus menu's A and X in the Dreamcast's red and
yellow, and the title's "Press [triangle] START", the triangle being the
Dreamcast's start button. The Xbox pad's A is green, its X blue, and its
START a plain button, so the Xbox build uses these, written once into
Platform/xbox/ (they are the Dreamcast port's own drawings, recoloured and
cut, not game data):

  xbox_btn_a.rgba16.inc.c       the A, red turned to the Xbox's green
  xbox_btn_x.rgba16.inc.c       the X, yellow turned to the Xbox's blue
  xbox_press_start.ia8.inc.c    "Press START": the triangle taken out and
                                the words closed up, centred in the same
                                120 x 13 so the title draws it unchanged

Run it again only to remake them; the build does not.
"""
import colorsys
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DC = os.path.join(ROOT, "src", "dc")
OUT = os.path.join(ROOT, "Platform", "xbox")


def read_words(name):
    with open(os.path.join(DC, name), encoding="utf-8") as f:
        return [int(v, 16) for v in re.findall(r"0x[0-9a-fA-F]+", f.read())]


def write_words(name, words, digits, per_line, source):
    lines = ["/* %s -- made by tools/xbox/make_xbox_buttons.py from src/dc/%s */" % (name, source)]
    fmt = "0x%%0%dx" % digits
    for i in range(0, len(words), per_line):
        lines.append(", ".join(fmt % w for w in words[i:i + per_line]) + ", ")
    with open(os.path.join(OUT, name), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def recolour(words, hue_from, hue_to):
    """RGBA5551: each opaque texel's hue moved by (hue_to - hue_from) degrees,
    its saturation and value kept, so the shading and the letter stay."""
    out = []
    shift = (hue_to - hue_from) / 360.0
    for w in words:
        if not (w & 1):
            out.append(w)
            continue
        r, g, b = ((w >> 11) & 31) / 31.0, ((w >> 6) & 31) / 31.0, ((w >> 1) & 31) / 31.0
        h, s, v = colorsys.rgb_to_hsv(r, g, b)
        r, g, b = colorsys.hsv_to_rgb((h + shift) % 1.0, s, v)
        out.append((round(r * 31) << 11) | (round(g * 31) << 6) | (round(b * 31) << 1) | 1)
    return out


def press_start():
    W = 120
    px = read_words("triPressStart.ia8.inc.c")
    H = len(px) // W

    def col_used(x):
        return any(px[y * W + x] & 0x0F for y in range(H))

    # the image is "Press" (columns 0-44), the triangle (45-58) and "START"
    # (59-113), read off the texels; the letters' antialiasing touches the
    # triangle's, so the columns are given, and checked: the triangle's tip
    # is the only thing in the top row, and the columns either side of it
    # are the words'
    TRI = (45, 59)
    apex = [x for x in range(W) if px[x] & 0x0F]
    if not apex or apex[0] < TRI[0] or apex[-1] >= TRI[1]:
        raise SystemExit("make_xbox_buttons: the triangle is not at columns %d-%d" % (TRI[0], TRI[1] - 1))
    if not (col_used(TRI[0] - 1) and col_used(TRI[1])):
        raise SystemExit("make_xbox_buttons: the words do not meet the triangle where expected")
    used = [x for x in range(W) if col_used(x)]
    press = (used[0], TRI[0])                    # columns of "Press"
    start = (TRI[1], used[-1] + 1)               # columns of "START"
    space = 4
    width = (press[1] - press[0]) + space + (start[1] - start[0])
    left = (W - width) // 2
    out = [0] * (W * H)
    for y in range(H):
        x = left
        for c in range(press[0], press[1]):
            out[y * W + x] = px[y * W + c]
            x += 1
        x += space
        for c in range(start[0], start[1]):
            out[y * W + x] = px[y * W + c]
            x += 1
    return out, press, start


def main():
    write_words("xbox_btn_a.rgba16.inc.c", recolour(read_words("red_a.rgba16.inc.c"), 0.0, 115.0), 4, 8,
                "red_a.rgba16.inc.c")
    write_words("xbox_btn_x.rgba16.inc.c", recolour(read_words("yellow_x.rgba16.inc.c"), 50.0, 215.0), 4, 8,
                "yellow_x.rgba16.inc.c")
    ps, press, start = press_start()
    write_words("xbox_press_start.ia8.inc.c", ps, 2, 16, "triPressStart.ia8.inc.c")
    print("make_xbox_buttons: A, X, Press START (Press %d-%d, START %d-%d)" % (press + start))


if __name__ == "__main__":
    main()
