#!/usr/bin/env python3
"""Build the asset segments: Torch's src/assets/ast_*/ast_*.c -> sf_data/*.bin.

The Dreamcast Makefile's `sf-data` rule compiles each asset file, LINKS it at
its N64 segment base (sh-elf-ld -Ttext=0x06000000 ...) and dumps .data+.bss
(objcopy -O binary). The game then loads those files into the segment buffers
at run time, and the main executable takes every asset symbol's address from
the linked ELFs (--just-symbols), so `aCoWaterfallDL` IS 0x06000E00.

This does the same with the Xbox's compiler: each file is compiled by RXDK's
clang with the game's own flags (xbconfig.py -- the data is read through the
game's struct definitions, so the layouts must match the game's), then laid
out at its base and relocated here (COFF has no -Ttext; the arithmetic is the
same). Outputs:

  data/sf_data/<name>.bin             what Load_RomFile reads (src/engine/fox_load.c)
  Platform/xbox/gen/asset_symbols.c   every asset symbol as an absolute symbol,
                                      in the Makefile's --just-symbols order
  Platform/xbox/gen/seg_sizes.h       the largest file each segment must hold

The four Titania segments (7_ti_2, 8_ti, 9_ti, A_ti) reference one another and
are linked as one group, as the Makefile's chain of -R links does.

Always rebuilt in full: every output is derived from Torch's output and the
compiler flags, and a stale segment is silently wrong, not missing.
"""
import concurrent.futures
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coff      # noqa: E402
import xbconfig  # noqa: E402

ROOT = xbconfig.ROOT
OBJ_DIR = os.path.join(ROOT, "build", "xbox_assets")
DATA_DIR = os.path.join(ROOT, "data", "sf_data")
GEN_DIR = os.path.join(ROOT, "Platform", "xbox", "gen")

# (asset, segment, file) -- the Makefile's sf-data rule, in its order.
SEGMENTS = [
    ("ast_text", 0x05, "text.bin"),
    ("ast_common", 0x01, "common.bin"),
    ("ast_bg_space", 0x02, "bgspace.bin"),
    ("ast_bg_planet", 0x02, "bgplanet.bin"),
    ("ast_arwing", 0x03, "arwing.bin"),
    ("ast_landmaster", 0x03, "landmstr.bin"),
    ("ast_blue_marine", 0x03, "blumarin.bin"),
    ("ast_versus", 0x03, "versus.bin"),
    ("ast_enmy_planet", 0x04, "enmyplnt.bin"),
    ("ast_enmy_space", 0x04, "enmyspce.bin"),
    ("ast_great_fox", 0x0E, "greatfox.bin"),
    ("ast_star_wolf", 0x0F, "starwolf.bin"),
    ("ast_allies", 0x0D, "allies.bin"),
    ("ast_corneria", 0x06, "corneria.bin"),
    ("ast_meteo", 0x06, "meteo.bin"),
    ("ast_titania", 0x06, "titania.bin"),
    ("ast_sector_x", 0x06, "sectorx.bin"),
    ("ast_sector_z", 0x06, "sectorz.bin"),
    ("ast_aquas", 0x06, "aquas.bin"),
    ("ast_area_6", 0x06, "area6.bin"),
    ("ast_venom_1", 0x06, "venom1.bin"),
    ("ast_venom_2", 0x06, "venom2.bin"),
    ("ast_ve1_boss", 0x09, "ve1boss.bin"),
    ("ast_bolse", 0x06, "bolse.bin"),
    ("ast_fortuna", 0x06, "fortuna.bin"),
    ("ast_sector_y", 0x06, "sectory.bin"),
    ("ast_solar", 0x06, "solar.bin"),
    ("ast_zoness", 0x06, "zoness.bin"),
    ("ast_katina", 0x06, "katina.bin"),
    ("ast_macbeth", 0x06, "macbeth.bin"),
    ("ast_warp_zone", 0x07, "warpzone.bin"),
    ("ast_title", 0x06, "title.bin"),
    ("ast_map", 0x06, "map.bin"),
    ("ast_option", 0x08, "option.bin"),
    ("ast_vs_menu", 0x07, "vsmenu.bin"),
    ("ast_font_3d", 0x09, "font3d.bin"),
    ("ast_andross", 0x0C, "andross.bin"),
    ("ast_logo", 0x0F, "logo.bin"),
    ("ast_ending", 0x07, "ending.bin"),
    ("ast_ending_award_front", 0x08, "awdfront.bin"),
    ("ast_ending_award_back", 0x08, "awdback.bin"),
    ("ast_ending_expert", 0x08, "endexprt.bin"),
    ("ast_training", 0x06, "training.bin"),
    ("ast_7_ti_1", 0x07, "7ti1.bin"),
    ("ast_7_ti_2", 0x07, "7ti2.bin"),
    ("ast_8_ti", 0x08, "8ti.bin"),
    ("ast_9_ti", 0x09, "9ti.bin"),
    ("ast_A_ti", 0x0A, "ati.bin"),
]

# linked together (they name each other's data)
TI_GROUP = ["ast_7_ti_2", "ast_8_ti", "ast_9_ti", "ast_A_ti"]

# The Makefile's ASSET_ELFS: the order of the main link's --just-symbols.
ASSET_ELFS = ["ast_common", "ast_bg_space", "ast_bg_planet", "ast_arwing", "ast_landmaster",
              "ast_blue_marine", "ast_versus", "ast_enmy_planet", "ast_enmy_space",
              "ast_great_fox", "ast_star_wolf", "ast_allies", "ast_corneria", "ast_meteo",
              "ast_titania", "ast_7_ti_2", "ast_8_ti", "ast_9_ti", "ast_A_ti", "ast_7_ti_1",
              "ast_sector_x", "ast_sector_z", "ast_aquas", "ast_area_6", "ast_venom_1",
              "ast_venom_2", "ast_ve1_boss", "ast_bolse", "ast_fortuna", "ast_sector_y",
              "ast_solar", "ast_zoness", "ast_katina", "ast_macbeth", "ast_warp_zone",
              "ast_title", "ast_map", "ast_option", "ast_vs_menu", "ast_text", "ast_font_3d",
              "ast_andross", "ast_logo", "ast_ending", "ast_ending_award_front",
              "ast_ending_award_back", "ast_ending_expert", "ast_training"]


def compile_one(name):
    src = os.path.join(ROOT, "src", "assets", name, name + ".c")
    obj = os.path.join(OBJ_DIR, name + ".obj")
    if not os.path.exists(src):
        return name, "missing %s (run Torch first: python setup.py)" % os.path.relpath(src, ROOT)
    if os.path.exists(obj):
        os.remove(obj)
    r = subprocess.run(xbconfig.compile_command(src, obj, xbconfig.ASSET_FLAGS),
                       cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        return name, r.stderr[-4000:]
    return name, None


class Placed(object):
    """One object laid out at a base: section -> address, and its bytes."""

    def __init__(self, name, base):
        self.name = name
        self.base = base
        self.obj = coff.CoffObject(os.path.join(OBJ_DIR, name + ".obj"))
        self.addr = {}
        self.image = bytearray()
        for sec in self.obj.sections:
            if sec.is_dropped or sec.size == 0:
                continue
            if sec.is_code:
                raise SystemExit("build_assets: %s has code in %s -- an asset file must be data only"
                                 % (name, sec.name))
            pad = (-len(self.image)) % sec.align
            self.image += bytes(pad)
            self.addr[sec.index] = base + len(self.image)
            self.image += sec.data if sec.data is not None else bytes(sec.size)

    def defined(self):
        out = {}
        for s in self.obj.symlist:
            if s.is_global_def and s.section in self.addr:
                out[s.name] = self.addr[s.section] + s.value
        return out

    def relocate(self, table):
        unresolved = set()
        for sec in self.obj.sections:
            if sec.index not in self.addr:
                continue
            at = self.addr[sec.index] - self.base
            for (va, si, typ) in sec.relocs:
                sym = self.obj.symbols[si]
                if sym.section > 0 and sym.section in self.addr:
                    target = self.addr[sym.section] + sym.value
                elif sym.section == coff.IMAGE_SYM_ABSOLUTE:
                    target = sym.value
                elif sym.name in table:
                    target = table[sym.name]
                else:
                    unresolved.add(sym.name)
                    continue
                off = at + va
                if typ == coff.IMAGE_REL_I386_DIR32:
                    addend = struct.unpack_from("<I", self.image, off)[0]
                    struct.pack_into("<I", self.image, off, (target + addend) & 0xFFFFFFFF)
                elif typ == coff.IMAGE_REL_I386_ABSOLUTE:
                    pass
                else:
                    raise SystemExit("build_assets: %s: relocation type %#x at %s+%#x is not one "
                                     "data can carry" % (self.name, typ, sec.name, va))
        return unresolved


def main():
    os.makedirs(OBJ_DIR, exist_ok=True)
    os.makedirs(DATA_DIR, exist_ok=True)
    os.makedirs(GEN_DIR, exist_ok=True)

    names = [s[0] for s in SEGMENTS]
    jobs = max(1, (os.cpu_count() or 2))
    failed = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        for name, err in ex.map(compile_one, names):
            if err:
                failed.append((name, err))
    if failed:
        for name, err in failed:
            sys.stderr.write("--- %s\n%s\n" % (name, err))
        raise SystemExit("build_assets: %d asset file(s) did not compile" % len(failed))

    seg_of = {n: seg for n, seg, _ in SEGMENTS}
    placed = {n: Placed(n, seg_of[n] << 24) for n in names}

    # the Titania group shares one symbol table; everything else stands alone
    group_table = {}
    for n in TI_GROUP:
        group_table.update(placed[n].defined())
    for n in names:
        table = group_table if n in TI_GROUP else {}
        unresolved = placed[n].relocate(table)
        if unresolved:
            raise SystemExit("build_assets: %s names symbols nothing defines: %s"
                             % (n, ", ".join(sorted(unresolved)[:10])))

    seg_max = {}
    for n, seg, fname in SEGMENTS:
        img = bytes(placed[n].image)
        path = os.path.join(DATA_DIR, fname)
        old = open(path, "rb").read() if os.path.exists(path) else None
        if old != img:
            with open(path, "wb") as f:
                f.write(img)
        seg_max[seg] = max(seg_max.get(seg, 0), len(img))

    # absolute symbols for the game: first definition wins, as --just-symbols
    abs_syms, owner = [], {}
    for n in ASSET_ELFS:
        for sym, addr in sorted(placed[n].defined().items(), key=lambda kv: kv[1]):
            if sym in owner:
                if abs_syms[owner[sym]][1] != addr:
                    raise SystemExit("build_assets: %s is defined by both %s and %s"
                                     % (sym, abs_syms[owner[sym]][2], n))
                continue
            owner[sym] = len(abs_syms)
            abs_syms.append((sym, addr, n))
    lines = ["/* GENERATED by tools/xbox/build_assets.py -- do not edit.",
             " *",
             " * Every asset symbol, at the segment address its file is loaded to: the",
             " * Dreamcast link's --just-symbols of the asset ELFs. An absolute symbol is",
             " * its address, with no storage behind it in the XBE. */",
             ""]
    cur = None
    for sym, addr, n in abs_syms:
        if n != cur:
            lines.append("/* %s */" % n)
            cur = n
        lines.append('__asm__(".globl %s\\n\\t.set %s, 0x%08x");' % (sym, sym, addr))
    write_text(os.path.join(GEN_DIR, "asset_symbols.c"), "\n".join(lines) + "\n")

    hdr = ["/* GENERATED by tools/xbox/build_assets.py -- do not edit.",
           " *",
           " * The largest sf_data file loaded into each segment, in bytes. The Xbox",
           " * segment buffers (Platform/xbox/xbox_segments.c) are sized from this. */",
           "#ifndef XBOX_GEN_SEG_SIZES_H",
           "#define XBOX_GEN_SEG_SIZES_H",
           "",
           "static const unsigned int xbox_seg_file_max[16] = {"]
    for s in range(16):
        hdr.append("    0x%06x,   /* segment %X */" % (seg_max.get(s, 0), s))
    hdr += ["};", "", "#endif", ""]
    write_text(os.path.join(GEN_DIR, "seg_sizes.h"), "\n".join(hdr))

    total = sum(len(placed[n].image) for n in names)
    print("build_assets: %d segments, %.1f MB of sf_data, %d asset symbols"
          % (len(names), total / 1048576.0, len(abs_syms)))


def write_text(path, text):
    old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
    if old != text:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)


if __name__ == "__main__":
    main()
