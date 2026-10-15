#!/usr/bin/env python3
"""Write rxdk.project.json from the Dreamcast Makefile's link list.

The Dreamcast Makefile's $(ELF) rule links the game from named object lists
(DC_OBJS, AUDIO_OBJS, GFX_OBJS, SYS_OBJS, LIBULTRA_OBJS, ENGINE_OBJS, the
OVERLAY_*_OBJS, MISC_OBJS, FINAL_OBJS). Those lists are the authority on what
the game is made of; this reads them out of the Makefile, so the Xbox source
list cannot drift from the Dreamcast one.

Three Dreamcast objects are not built for the Xbox:

  build/dcconsole.o, build/dclogo.o  -- a Dreamcast logo and console picture
      (assets/dreamcast/*.png through KOS's pvrtex and bin2o). The Xbox build
      draws the game's own N64 logo and console from the ROM's assets instead
      (src/overlays/ovl_menu/fox_title.c, fox_option.c, XBOX: blocks).

  build/src/sys/sys_fault.o  -- the N64 crash screen. Nothing outside the
      file refers to anything in it (Fault_Init is never called), and kos-cc
      links with --gc-sections, so the Dreamcast executable holds none of it;
      it also calls libultra's osCreateThread/osStartThread, which the port
      never implemented. RXDK's link does not discard unreferenced code, so
      the file is left out here to the same effect.

and the Xbox adds its platform layer (Platform/xbox/*.c, the KallistiOS the
game is written against) and the asset symbols (Platform/xbox/gen/
asset_symbols.c, tools/xbox/build_assets.py). Compile settings come from
tools/xbox/xbconfig.py, shared with the asset build.

    python tools/xbox/gen_manifest.py
"""
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xbconfig  # noqa: E402

ROOT = xbconfig.ROOT
PROJECT_NAME = "Star-Fox-64-X"      # the ISO is out/<Config>/XISO/Star-Fox-64-X.iso
TITLE_ID = "0x54520012"             # Team Resurgent TitleID-DB: next after SSB-X (54520011)
TITLE_NAME = "Star Fox 64"

LIST_VARS = ["DC_OBJS", "AUDIO_OBJS", "GFX_OBJS", "SYS_OBJS", "LIBULTRA_OBJS", "ENGINE_OBJS",
             "OVERLAY_I1_OBJS", "OVERLAY_I2_OBJS", "OVERLAY_I3_OBJS", "OVERLAY_I4_OBJS",
             "OVERLAY_I5_OBJS", "OVERLAY_I6_OBJS", "OVERLAY_MENU_OBJS", "OVERLAY_ENDING_OBJS",
             "OVERLAY_UNUSED_OBJS", "MISC_OBJS", "FINAL_OBJS"]

# Dreamcast objects with no Xbox counterpart to build (see the docstring).
DROP = {"build/dcconsole.o", "build/dclogo.o", "build/src/sys/sys_fault.o"}

LIBS_RELEASE = ["libd3d8.lib", "libd3dx8.lib", "libxgraphics.lib", "libdsound.lib",
                "libxapi.lib", "libc.lib", "libcpp.lib", "libkernel.lib"]
LIBS_DEBUG = ["libxbdmd.lib", "libd3d8d.lib", "libd3dx8d.lib", "libxgraphicsd.lib",
              "libdsoundd.lib", "libxapid.lib", "libcd.lib", "libcppd.lib", "libkerneld.lib"]


def makefile_objects():
    """The objects of the Makefile's link, in link order."""
    text = open(os.path.join(ROOT, "Makefile"), encoding="utf-8").read()
    text = text.replace("\\\n", " ")
    lists = {}
    for line in text.splitlines():
        m = re.match(r"^([A-Z0-9_]+)\s*:=\s*(.*)$", line)
        if m and m.group(1) in LIST_VARS + ["OVERLAY_OBJS"]:
            lists[m.group(1)] = m.group(2).split()
    missing = [v for v in LIST_VARS if v not in lists]
    if missing:
        raise SystemExit("gen_manifest: the Makefile has no %s" % ", ".join(missing))
    out = []
    for v in LIST_VARS:
        out += lists[v]
    return out


def object_to_source(obj):
    if not obj.startswith("build/") or not obj.endswith(".o"):
        raise SystemExit("gen_manifest: %s is not a build/*.o object" % obj)
    src = obj[len("build/"):-2] + ".c"
    if not os.path.isfile(os.path.join(ROOT, src)):
        raise SystemExit("gen_manifest: %s has no source %s" % (obj, src))
    return src


def platform_sources():
    out = []
    base = os.path.join(ROOT, "Platform", "xbox")
    for dp, dn, fn in os.walk(base):
        dn[:] = sorted(d for d in dn if d != "gen")
        for f in sorted(fn):
            # .inc.c is data #included by a source (the button art), not one itself
            if f.endswith(".c") and not f.endswith(".inc.c"):
                out.append(os.path.relpath(os.path.join(dp, f), ROOT).replace("\\", "/"))
    return sorted(out)


def main():
    objs = makefile_objects()
    game = [object_to_source(o) for o in objs if o not in DROP]
    sources = platform_sources() + ["Platform/xbox/gen/asset_symbols.c"] + game

    # RXDK's Visual Studio build names each object after its file alone
    seen = {}
    for s in sources:
        b = os.path.basename(s).lower()
        if b in seen:
            raise SystemExit("gen_manifest: %s and %s share a file name; RXDK's Visual Studio "
                             "build would overwrite one object with the other" % (seen[b], s))
        seen[b] = s

    def cfg(debug):
        return {
            "configuration": "debug" if debug else "release",
            # the dashboard icons: BMPs (tools/xbox/make_icons.py) bundled
            # to DXT1 .xbx before the link
            "resources": ["Platform/xbox/titleimage.rdf", "Platform/xbox/saveimage.rdf"],
            "sources": sources,
            "libraries": LIBS_DEBUG if debug else LIBS_RELEASE,
            "includePaths": xbconfig.INCLUDE_PATHS,
            "defines": xbconfig.DEFINES,
            "compileFlags": xbconfig.COMPILE_FLAGS,
            "cppStandard": "c++20",
            "exceptions": False,
            "deployPaths": ["data"],
            "forceCopy": True,
            "createIso": True,
            "outputDir": "out/Debug" if debug else "out/Release",
            "imageBuild": {
                "stackSize": 262144,
                "debug": debug,
                "noLogo": True,
                "noLibWarn": True,
                "testId": TITLE_ID,
                "testName": TITLE_NAME,
                "testVersion": "1",
                "titleImage": "Platform/xbox/gen/titleimage.xbx",
                "defaultSaveImage": "Platform/xbox/gen/saveimage.xbx",
            },
        }

    manifest = {
        "name": PROJECT_NAME,
        "type": "executable",
        "defaultConfiguration": "Release",
        "configurations": {"Debug": cfg(True), "Release": cfg(False)},
    }
    out = os.path.join(ROOT, "rxdk.project.json")
    text = json.dumps(manifest, indent=2) + "\n"
    old = open(out, encoding="utf-8").read() if os.path.isfile(out) else None
    if old != text:            # unchanged, it keeps its time (the RXDK extension watches it)
        with open(out, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    print("gen_manifest: %d sources (%d game, %d platform, %d Dreamcast objects dropped)"
          % (len(sources), len(game), len(sources) - len(game),
             len([o for o in objs if o in DROP])))


if __name__ == "__main__":
    main()
