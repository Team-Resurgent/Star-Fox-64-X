#!/usr/bin/env python3
"""Decompress the Star Fox 64 ROM, the way `make decompress` does.

The Dreamcast Makefile runs tools/comptool.py -dse baserom -m tools/mio0,
which shells out to the native mio0 tool and to `rm` for every file -- neither
of which a plain Windows console has. This is the same job in Python:

  * walks the ROM's DMA table (found by the same signature comptool uses),
  * MIO0-decodes every compressed file (the decoder is tools/mio0-decompressor's
    mio0_decode, line for line),
  * writes the uncompressed ROM with its DMA table rewritten and its CRCs fixed
    (comptool's calculate_crcs), which is what Torch reads,
  * and writes each file to <extract>/<name>.us.rev1.bin, which is where the
    AICA sample tool reads audio_bank / audio_table from.

The output is checked against the uncompressed md5 the repository carries
(starfox64.us.rev1.uncompressed.md5): a wrong ROM is refused, not half-built.

Usage: decompress_rom.py <in.z64> <out.uncompressed.z64> <extract dir>
"""
import hashlib
import os
import struct
import sys

# comptool.py's US file list, in DMA-table order.
FILE_NAMES_US = [
    "makerom", "main", "dma_table", "audio_seq", "audio_bank", "audio_table", "ast_common", "ast_bg_space", "ast_bg_planet",
    "ast_arwing", "ast_landmaster", "ast_blue_marine", "ast_versus", "ast_enmy_planet", "ast_enmy_space", "ast_great_fox",
    "ast_star_wolf", "ast_allies", "ast_corneria", "ast_meteo", "ast_titania", "ast_7_ti_2", "ast_8_ti", "ast_9_ti", "ast_A_ti",
    "ast_7_ti_1", "ast_sector_x", "ast_sector_z", "ast_aquas", "ast_area_6", "ast_venom_1", "ast_venom_2", "ast_ve1_boss",
    "ast_bolse", "ast_fortuna", "ast_sector_y", "ast_solar", "ast_zoness", "ast_katina", "ast_macbeth", "ast_warp_zone",
    "ast_title", "ast_map", "ast_option", "ast_vs_menu", "ast_text", "ast_font_3d", "ast_andross", "ast_logo", "ast_ending",
    "ast_ending_award_front", "ast_ending_award_back", "ast_ending_expert", "ast_training", "ast_radio", "ovl_i1", "ovl_i2",
    "ovl_i3", "ovl_i4", "ovl_i5", "ovl_i6", "ovl_menu", "ovl_ending", "ovl_unused",
]
US_REV1_DMA_TABLE = 0xDE480


def mio0_decode(buf):
    """tools/mio0-decompressor/libmio0.c mio0_decode."""
    if buf[:4] != b"MIO0":
        raise ValueError("not an MIO0 block")
    dest_size, comp_off, uncomp_off = struct.unpack_from(">III", buf, 4)
    out = bytearray(dest_size)
    bit_idx = 0
    idx = 0
    comp = comp_off
    uncomp = uncomp_off
    layout = 16
    while idx < dest_size:
        if buf[layout + (bit_idx >> 3)] & (0x80 >> (bit_idx & 7)):
            out[idx] = buf[uncomp]
            idx += 1
            uncomp += 1
        else:
            vals = (buf[comp] << 8) | buf[comp + 1]
            comp += 2
            length = ((vals >> 12) & 0xF) + 3
            offset = (vals & 0xFFF) + 1
            src = idx - offset
            for i in range(length):
                out[idx] = out[src + i]
                idx += 1
        bit_idx += 1
    return bytes(out)


def _u32(x):
    return x & 0xFFFFFFFF


def _rol(i, b):
    return _u32((i << b) | (i >> (32 - b)))


def calculate_crcs(buf, seed=0xF8CA4DDC, start=0x1000, end=0x101000):
    """tools/comptool.py calculate_crcs (the CIC-6102 checksum)."""
    t1 = t2 = t3 = t4 = t5 = t6 = seed
    for (d,) in struct.iter_unpack(">I", buf[start:end]):
        if _u32(t6 + d) < t6:
            t4 = _u32(t4 + 1)
        t6 = _u32(t6 + d)
        t3 ^= d
        r = _rol(d, d & 0x1F)
        t5 = _u32(t5 + r)
        if t2 > d:
            t2 ^= r
        else:
            t2 ^= t6 ^ d
        t1 = _u32(t1 + (t5 ^ d))
    return _u32(t6 ^ t4 ^ t3), _u32(t5 ^ t2 ^ t1)


def find_file_table(rom):
    for sig in (b"\x00" * 10 + b"\x10\x50" + b"\x00" * 4, b"\x00" * 10 + b"\x10\x60" + b"\x00" * 4):
        at = rom.find(sig)
        if at != -1:
            return at
    raise SystemExit("decompress_rom: the ROM's DMA table was not found")


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    src, dst, extract = sys.argv[1:]
    rom = open(src, "rb").read()
    if rom[0x20:0x24] != b"STAR":
        raise SystemExit("decompress_rom: %s is not a big-endian (.z64) Star Fox 64 ROM" % src)
    if rom[0x3E:0x40] != b"E\x01":
        raise SystemExit("decompress_rom: %s is not the US v1.1 ROM" % src)
    table = find_file_table(rom)
    if table != US_REV1_DMA_TABLE:
        print("decompress_rom: warning, DMA table at 0x%X (expected 0x%X)" % (table, US_REV1_DMA_TABLE))

    os.makedirs(extract, exist_ok=True)
    out = bytearray()
    n = 0
    while True:
        entry = table + 0x10 * n
        vstart, pstart, pend, comp = struct.unpack_from(">IIII", rom, entry)
        if vstart == 0 and pend == 0:
            break
        data = rom[pstart:pend]
        if comp == 1:
            data = mio0_decode(data)
        elif comp != 0:
            raise SystemExit("decompress_rom: bad compression flag %d in file %d" % (comp, n))
        del out[vstart:]
        if len(out) < vstart:
            out += bytes(vstart - len(out))
        out += data
        name = FILE_NAMES_US[n] if n < len(FILE_NAMES_US) else "file%02d" % n
        with open(os.path.join(extract, "%s.us.rev1.bin" % name), "wb") as f:
            f.write(data)
        # comptool seeks to the entry and writes it even when the output has
        # not reached the table yet (the file grows with zeros, and the next
        # file's truncate cuts it back off). Reproduced exactly, so entries
        # written before the dma_table file itself arrives are lost the same
        # way and the result matches the reference md5.
        if len(out) < entry + 16:
            out += bytes(entry + 16 - len(out))
        struct.pack_into(">III", out, entry + 4, vstart, vstart + len(data), 0)
        n += 1
    crc1, crc2 = calculate_crcs(bytes(out))
    struct.pack_into(">II", out, 0x10, crc1, crc2)

    md5_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                            "starfox64.us.rev1.uncompressed.md5")
    got = hashlib.md5(out).hexdigest()
    if os.path.exists(md5_file):
        want = open(md5_file).read().split()[0].lower()
        if got != want:
            raise SystemExit("decompress_rom: uncompressed ROM md5 %s, expected %s" % (got, want))
    with open(dst, "wb") as f:
        f.write(out)
    print("decompress_rom: %d files, %s (md5 %s)" % (n, dst, got))


if __name__ == "__main__":
    main()
