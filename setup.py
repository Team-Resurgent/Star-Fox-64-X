#!/usr/bin/env python3
"""One command from a ROM to an Xbox disc image.

    python setup.py                 # everything -> out/Release/XISO/Star-Fox-64-X.iso
    python setup.py --check         # say what is there and what is missing
    python setup.py --skip-build    # stop before compiling (to build in Visual Studio)
    python setup.py --config Debug  # the debug configuration

Needs: Python 3.8+ (standard library only), git, CMake and Visual Studio 2022
or later with its C++ tools (to build Torch, the asset extractor, which
setup.py fetches at the revision below, and whose own build downloads its
libraries -- so the first run needs the internet), the RXDK toolchain, and
your US v1.1 Star Fox 64 ROM (.z64) in this folder as baserom.us.rev1.z64
(or baserom.z64, which is copied to that name).

The steps, in order. None is skipped because its output already exists: a
derived file gated on its own existence goes stale silently. Each one either
runs every time, or is driven by a tool that knows its own inputs (CMake for
Torch).

  1. the ROM, checked against starfox64.us.rev1.md5, and decompressed
     (tools/xbox/decompress_rom.py: the uncompressed ROM Torch reads, and the
     audio files under baserom/)
  2. Torch, fetched into tools/Torch at TORCH_REV and built there (CMake,
     incremental)
  3. the assets, as the Makefile's `assets` rule makes them: Torch's code,
     header and modding export into src/assets and include/assets, then
     tools/strip_dc_syncs.py; and the Dreamcast port's radio messages
     (src/dc/ast_radio.c: the extracted ones with its button-name edits,
     tools/xbox/radio_messages.py)
  4. the audio, as its `sf-data` rule: the Yamaha-ADPCM sample pool and table
     (tools/aica/sf64_emit.py, its C encoder built with RXDK's host clang),
     and the sequence and bank files
  5. the Xbox's own: the asset segments (tools/xbox/build_assets.py), the
     dashboard icons (make_icons.py), rxdk.project.json (gen_manifest.py)
     and the Visual Studio project (gen_vcxproj.py)
  6. the build: Rxdk.Cli compiles, links, bundles the icons and packs the ISO
"""
import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT, "tools", "xbox"))
import xbconfig  # noqa: E402

ROM = os.path.join(ROOT, "baserom.us.rev1.z64")
ROM_ALT = os.path.join(ROOT, "baserom.z64")
ROM_UNCOMPRESSED = os.path.join(ROOT, "baserom.us.rev1.uncompressed.z64")
ROM_MD5_FILE = os.path.join(ROOT, "starfox64.us.rev1.md5")
SF_DATA = os.path.join(ROOT, "data", "sf_data")
TORCH_SRC = os.path.join(ROOT, "tools", "Torch")
# HarbourMasters/Torch at the revision the Dreamcast port pins as its
# submodule (sf64-dc's tools/Torch): the asset layout the game is built for
TORCH_URL = "https://github.com/HarbourMasters/Torch"
TORCH_REV = "6ca699d508d2b62ab51d290e6a38a1a24f7d83cf"
TORCH_BUILD = os.path.join(ROOT, "build", "torch")
TORCH_EXE = os.path.join(TORCH_BUILD, "Release", "torch.exe")
YA2BEAM_SRC = os.path.join(ROOT, "tools", "aica", "ya2beam.c")
YA2BEAM_SO = os.path.join(ROOT, "tools", "aica", "_ya2beam.so")
NAME = "Star-Fox-64-X"


def say(msg):
    print("setup: " + msg, flush=True)


def die(msg):
    print("setup: ERROR: " + msg, file=sys.stderr, flush=True)
    sys.exit(1)


def run(cmd, cwd=ROOT, env=None):
    t = time.time()
    r = subprocess.run(cmd, cwd=cwd, env=env)
    if r.returncode != 0:
        die("failed (%d): %s" % (r.returncode, " ".join(cmd)))
    return time.time() - t


def tool(name, *args):
    run([sys.executable, os.path.join(ROOT, "tools", "xbox", name)] + list(args))


# ------------------------------------------------------------------ checks

def rom_md5():
    with open(ROM_MD5_FILE, encoding="utf-8") as f:
        return f.read().split()[0].lower()


def rom_ok():
    if not os.path.isfile(ROM):
        if not os.path.isfile(ROM_ALT):
            return False, ("no ROM: put your US v1.1 Star Fox 64 ROM (.z64) here as "
                           "baserom.us.rev1.z64")
        src = ROM_ALT
    else:
        src = ROM
    with open(src, "rb") as f:
        data = f.read()
    md5 = hashlib.md5(data).hexdigest()
    if md5 != rom_md5():
        hint = ""
        if data[:4] in (b"\x37\x80\x40\x12", b"\x40\x12\x37\x80"):
            hint = " (it is byte-swapped: a .v64/.n64 dump; convert it to .z64)"
        return False, "%s is not the US v1.1 ROM: md5 %s, expected %s%s" % (
            os.path.basename(src), md5, rom_md5(), hint)
    if src != ROM:
        shutil.copyfile(src, ROM)
    return True, ""


def find_vs():
    """The newest Visual Studio (2022 or later) with the x64 C++ tools, by
    vswhere, which every Visual Studio installer puts in the same place."""
    vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
                           "Microsoft Visual Studio", "Installer", "vswhere.exe")
    if not os.path.isfile(vswhere):
        return None
    r = subprocess.run([vswhere, "-latest", "-products", "*", "-version", "[17.0,",
                        "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                        "-property", "installationPath"], capture_output=True, text=True)
    path = r.stdout.strip().splitlines()
    return path[0] if (r.returncode == 0 and path and os.path.isdir(path[0])) else None


def find_cmake():
    c = shutil.which("cmake")
    if c:
        return c
    vs = find_vs()
    if vs:
        p = os.path.join(vs, "Common7", "IDE", "CommonExtensions", "Microsoft", "CMake", "CMake",
                         "bin", "cmake.exe")
        if os.path.isfile(p):
            return p
    return None


def torch_rev():
    if not os.path.isdir(os.path.join(TORCH_SRC, ".git")) and not os.path.isfile(os.path.join(TORCH_SRC, ".git")):
        return None
    r = subprocess.run(["git", "-C", TORCH_SRC, "rev-parse", "HEAD"], capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else None


def check():
    ok = True
    good, why = rom_ok()
    say("ROM: %s" % ("US v1.1, ok" if good else why))
    ok &= good
    for name, path in (("RXDK CLI", xbconfig.rxdk_cli()), ("RXDK clang", xbconfig.clang())):
        there = os.path.isfile(path)
        say("%s: %s" % (name, path if there else "missing (RXDK > RXDK Window > Install Prerequisites)"))
        ok &= there
    py = sys.version_info >= (3, 8)
    say("Python: %d.%d%s" % (sys.version_info[0], sys.version_info[1], "" if py else " (3.8 or later needed)"))
    ok &= py
    git = shutil.which("git")
    say("git: %s" % (git or "missing (install Git for Windows)"))
    ok &= git is not None
    vs = find_vs()
    say("Visual Studio C++ tools: %s" % (vs or "missing (Visual Studio 2022 or later, Desktop development with C++)"))
    ok &= vs is not None
    cm = find_cmake()
    say("CMake: %s" % (cm or "missing (install CMake, or Visual Studio's C++ CMake tools)"))
    ok &= cm is not None
    rev = torch_rev()
    say("Torch source: %s" % ("at %s" % rev[:12] if rev == TORCH_REV else
                              ("at %s, will be moved to %s" % (rev[:12], TORCH_REV[:12]) if rev else
                               "not fetched yet (the build fetches it)")))
    for label, path in (("Torch", TORCH_EXE), ("assets", os.path.join(ROOT, "src", "assets")),
                        ("data", SF_DATA), ("manifest", os.path.join(ROOT, "rxdk.project.json"))):
        say("%s: %s" % (label, "built" if os.path.exists(path) else "not built yet"))
    return ok


# ------------------------------------------------------------------- steps

def step_rom():
    say("decompressing the ROM")
    tool("decompress_rom.py", ROM, ROM_UNCOMPRESSED, os.path.join(ROOT, "baserom"))


def fetch_torch():
    """Torch at TORCH_REV in tools/Torch: cloned the first time, moved to the
    revision when it is elsewhere (a checkout made as sf64-dc's submodule is
    at it already)."""
    if shutil.which("git") is None:
        die("git not found (install Git for Windows)")
    rev = torch_rev()
    if rev == TORCH_REV:
        return
    if rev is None:
        if os.path.isdir(TORCH_SRC) and os.listdir(TORCH_SRC):
            die("tools/Torch is there but is not a git checkout: delete it and run again")
        say("fetching Torch")
        run(["git", "clone", TORCH_URL, TORCH_SRC])
    else:
        run(["git", "-C", TORCH_SRC, "fetch", "origin"])
    run(["git", "-C", TORCH_SRC, "checkout", "--detach", TORCH_REV])
    run(["git", "-C", TORCH_SRC, "submodule", "update", "--init", "--recursive"])


def step_torch(jobs):
    cmake = find_cmake()
    if cmake is None:
        die("CMake not found (install CMake, or Visual Studio's C++ CMake tools)")
    vs = find_vs()
    if vs is None:
        die("Visual Studio 2022 or later with the C++ tools not found (Desktop development with C++)")
    fetch_torch()
    if not os.path.isfile(os.path.join(TORCH_BUILD, "CMakeCache.txt")):
        say("configuring Torch (its libraries are downloaded the first time)")
        # CMake picks its newest Visual Studio generator; the instance is the one found
        run([cmake, "-S", TORCH_SRC, "-B", TORCH_BUILD, "-A", "x64",
             "-DCMAKE_GENERATOR_INSTANCE=" + vs])
    say("building Torch (incremental)")
    run([cmake, "--build", TORCH_BUILD, "--config", "Release", "--parallel", str(jobs)])


def step_assets():
    for d in ("src/assets", "include/assets"):
        os.makedirs(os.path.join(ROOT, d), exist_ok=True)
    # Torch skips work its hash file says is done; the outputs are rebuilt
    # every time instead (the Makefile's clean removes it the same way).
    hashfile = os.path.join(ROOT, "torch.hash.yml")
    if os.path.isfile(hashfile):
        os.remove(hashfile)
    for verb in (["code"], ["header"], ["modding", "export"]):
        say("torch %s" % " ".join(verb))
        run([TORCH_EXE] + verb + [ROM_UNCOMPRESSED])
    say("stripping RDP syncs from the generated display lists")
    run([sys.executable, os.path.join(ROOT, "tools", "strip_dc_syncs.py"), "."])
    say("the radio messages, with the Dreamcast port's button names")
    tool("radio_messages.py")


def build_ya2beam():
    """tools/aica/ya2beam_c.py loads _ya2beam.so through ctypes, building it
    with `cc` when it is older than ya2beam.c; Windows has no `cc`, so it is
    built here with RXDK's host clang as a 64-bit DLL for this Python. If it
    fails, sf64_emit falls back to its pure-Python encoder (byte-identical,
    slower)."""
    llvm = xbconfig.llvm_root()
    sysroot = os.path.join(llvm, "host-sysroot")
    resdir = os.path.join(sysroot, "lib", "clang", "23")
    tmp = YA2BEAM_SO + ".tmp"
    cmd = [xbconfig.clang(), "--target=x86_64-w64-mingw32", "--sysroot=" + sysroot,
           "-resource-dir", resdir, "-fuse-ld=lld", "-rtlib=compiler-rt",
           "-unwindlib=libunwind", "-O3", "-shared", "-o", tmp, YA2BEAM_SRC]
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.isfile(tmp):
        say("could not build the C ADPCM encoder (%s); the pure-Python one is used"
            % (r.stderr.strip().splitlines() or ["?"])[-1])
        if os.path.isfile(YA2BEAM_SO):
            os.remove(YA2BEAM_SO)
        return
    os.replace(tmp, YA2BEAM_SO)
    for junk in (tmp[:-4] + ".lib", tmp[:-4] + ".exp"):
        if os.path.isfile(junk):
            os.remove(junk)


def step_audio():
    os.makedirs(SF_DATA, exist_ok=True)
    say("building the C ADPCM encoder")
    build_ya2beam()
    say("encoding the sample pool (tools/aica/sf64_emit.py)")
    env = dict(os.environ, PYTHONPATH=os.path.join(ROOT, "tools", "aica"))
    run([sys.executable, os.path.join(ROOT, "tools", "aica", "sf64_emit.py"),
         "--audiobank", os.path.join(ROOT, "baserom", "audio_bank.us.rev1.bin"),
         "--audiotable", os.path.join(ROOT, "baserom", "audio_table.us.rev1.bin"),
         "--tables", os.path.join(ROOT, "tools", "aica", "audio_tables.json"),
         "--outdir", os.path.join(ROOT, "src", "audio"),
         "--incdir", os.path.join(ROOT, "src", "audio"),
         "--pool", os.path.join(SF_DATA, "adpcm_pool.bin")], env=env)
    say("copying the sequence and bank files")
    shutil.copyfile(os.path.join(ROOT, "baserom", "audio_bank.us.rev1.bin"),
                    os.path.join(SF_DATA, "audbank.bin"))
    shutil.copyfile(os.path.join(ROOT, "baserom", "audio_seq.us.rev1.bin"),
                    os.path.join(SF_DATA, "audseq.bin"))


def step_xbox():
    say("laying out the asset segments")
    tool("build_assets.py")
    tool("make_icons.py")
    # the VMU save icon the game loads at run time (src/ultra_reimpl.c)
    shutil.copyfile(os.path.join(ROOT, "sf64.ico"), os.path.join(ROOT, "data", "sf64.ico"))
    tool("gen_manifest.py")
    tool("gen_vcxproj.py")


def step_build(config):
    cli = xbconfig.rxdk_cli()
    if not os.path.isfile(cli):
        die("Rxdk.Cli.exe not found: install RXDK (RXDK > RXDK Window > Install Prerequisites)")
    say("building %s" % config)
    run([cli, "build", "--project-root", ROOT, "--configuration", config])
    iso = os.path.join(ROOT, "out", config, "XISO", NAME + ".iso")
    if os.path.isfile(iso):
        say("done: %s (%.1f MB)" % (iso, os.path.getsize(iso) / 1048576.0))
    else:
        say("built; the ISO is under out/%s/XISO" % config)


def main():
    ap = argparse.ArgumentParser(description="ROM -> Xbox disc image for Star Fox 64")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--config", default="Release", choices=["Release", "Debug"])
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    a = ap.parse_args()
    if sys.version_info < (3, 8):
        die("Python 3.8 or later is needed")
    if a.check:
        sys.exit(0 if check() else 1)

    good, why = rom_ok()
    if not good:
        die(why)
    step_rom()
    step_torch(a.jobs)
    step_assets()
    step_audio()
    step_xbox()
    if not a.skip_build:
        step_build(a.config)


if __name__ == "__main__":
    main()
