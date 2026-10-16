"""The Xbox build's single description of how Star Fox 64 is compiled.

Two tools compile the same C: tools/xbox/build_assets.py (the asset segments,
which become data files) and RXDK itself (the game, from the manifest that
tools/xbox/gen_manifest.py writes). The assets are DATA the game reads through
its own struct definitions -- Gfx, Vtx, Animation, the object tables -- so the
two must agree on every layout. They do because both take their defines,
include paths and flags from here, and build_assets.py runs the very clang
RXDK runs, with RXDK's own fixed arguments (RXDK_BASE_FLAGS, copied from its
build log) in front.

The defines are the Dreamcast Makefile's (CFLAGS, BUILD_DEFINES, COMMON_,
RELEASE_, GBI_ and C_DEFINES), plus TARGET_XBOX.
"""
import glob
import os
import shutil
import subprocess
import tempfile
import threading

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def rxdk_root():
    """%ProgramData%\\RXDK, or $RXDK_ROOT."""
    if os.environ.get("RXDK_ROOT"):
        return os.environ["RXDK_ROOT"]
    pd = os.environ.get("ProgramData") or r"C:\ProgramData"
    return os.path.join(pd, "RXDK")


def rxdk_cli():
    staged = os.environ.get("RXDK_STAGED_TOOLS")
    if staged:
        return os.path.join(staged, "Rxdk.Cli.exe")
    return os.path.join(rxdk_root(), "tools", "Rxdk.Cli.exe")


def _rxdk_roots():
    """Where an RXDK keeps its parts: its root (%ProgramData%\\RXDK), and the
    per-user one (%LocalAppData%\\RXDK)."""
    roots = [rxdk_root()]
    local = os.environ.get("LOCALAPPDATA")
    if local:
        roots.append(os.path.join(local, "RXDK"))
    return [r for r in roots if os.path.isdir(r)]


_clang_cache = []


def clang():
    """The RXDK's own clang. Its place has moved between RXDK versions
    (RXDK 1.2.x: llvm\\xboxog-windows-x64\\bin), so it is looked for rather
    than assumed: $RXDK_CLANG, the known layouts, then any clang.exe under
    the RXDK's folders. A path that does not exist when there is none."""
    if _clang_cache:
        return _clang_cache[0]
    found = None
    env = os.environ.get("RXDK_CLANG", "").strip()
    if env and os.path.isfile(env):
        found = env
    if found is None:
        for root in _rxdk_roots():
            for pattern in (os.path.join(root, "llvm", "*", "bin", "clang.exe"),
                            os.path.join(root, "llvm", "bin", "clang.exe"),
                            os.path.join(root, "tools", "llvm", "*", "bin", "clang.exe"),
                            os.path.join(root, "tools", "llvm", "bin", "clang.exe")):
                hits = sorted(glob.glob(pattern))
                if hits:
                    found = hits[-1]
                    break
            if found:
                break
    if found is None:
        for root in _rxdk_roots():
            for dirpath, dirnames, filenames in os.walk(root):
                if dirpath[len(root):].count(os.sep) >= 5:
                    dirnames[:] = []
                if "clang.exe" in filenames:
                    found = os.path.join(dirpath, "clang.exe")
                    break
            if found:
                break
    if found is None:
        found = os.path.join(rxdk_root(), "llvm", "xboxog-windows-x64", "bin", "clang.exe")
    _clang_cache.append(found)
    return found


def llvm_root():
    """The LLVM install the RXDK's clang is in (the folder above its bin)."""
    return os.path.dirname(os.path.dirname(clang()))


def clang_resource_include():
    """clang's builtin headers, as clang itself reports them."""
    c = clang()
    if not os.path.isfile(c):
        return None
    try:
        r = subprocess.run([c, "-print-resource-dir"], capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
        inc = os.path.join(r.stdout.strip(), "include")
        if r.returncode == 0 and os.path.isdir(inc):
            return inc
    except OSError:
        pass
    hits = sorted(glob.glob(os.path.join(llvm_root(), "lib", "clang", "*", "include")))
    return hits[-1] if hits else None


ZIG_VERSION = "0.16.0"   # the Zig the RXDK pins (Rxdk.Engine ZigRuntime.ZigVersion)


def zig():
    """The RXDK's Zig, found as the RXDK finds it: $RXDK_ZIG, then its managed
    install (%LocalAppData%\\RXDK\\zig\\<version>), then zig on the PATH."""
    env = os.environ.get("RXDK_ZIG", "").strip()
    if env and os.path.isfile(env):
        return env
    base = os.path.join(os.environ.get("LOCALAPPDATA", ""), "RXDK", "zig", ZIG_VERSION)
    for cand in (os.path.join(base, "zig-x86_64-windows-" + ZIG_VERSION, "zig.exe"),
                 os.path.join(base, "zig.exe")):
        if os.path.isfile(cand):
            return cand
    return shutil.which("zig")


def c_compiler():
    """The C compiler the RXDK builds with, as the start of a command line
    that targets the Xbox, or None. Two kinds of RXDK exist: one carries its
    own clang (llvm/ under the RXDK root, RXDK 1.2.x) and passes clang's
    builtin headers back in after -nostdinc; older ones ran `zig cc` (Zig
    brings its own), kept here as the last resort. Either is the same clang,
    so the asset segments come out the same (checked byte for byte).
    $SF64X_COMPILER (clang or zig) picks one, for testing."""
    want = os.environ.get("SF64X_COMPILER", "").strip().lower()
    res = clang_resource_include()
    if want in ("", "clang") and os.path.isfile(clang()) and res:
        return [clang(), "-target", "i686-pc-windows-gnu", "-isystem", res]
    z = zig() if want in ("", "zig") else None
    if z:
        return [z, "cc", "-target", "x86-windows-gnu"]
    return None


def sdk_include():
    return os.path.join(rxdk_root(), "sdk", "include")


# What Rxdk.Cli puts in front of every C compile (its build log, RXDK 1.2.8),
# minus the optimisation level and debug info, which do not change a layout.
def rxdk_base_flags():
    return ["-fno-sanitize=undefined", "-ffreestanding", "-fno-stack-protector",
            "-fms-extensions", "-fms-compatibility", "-nostdinc",
            "-include", "picolibc.h", "-march=pentium3",
            "-D_XBOX", "-DXBOX", "-fno-builtin", "-U_DEBUG", "-D__ASSERT_VERBOSE",
            "-femulated-tls",
            "-I", sdk_include(),
            "-Wno-c++11-narrowing", "-Wno-address-of-temporary",
            "-Wno-ignored-pragma-intrinsic", "-Wno-multichar",
            "-Wno-unused-command-line-argument", "-Wno-deprecated-enum-enum-conversion"]


# The Dreamcast Makefile's defines.
DEFINES = [
    "TARGET_XBOX=1",
    "COMPILER_GCC",
    "NON_MATCHING=1",
    "AVOID_UB=1",
    "GBI_FLOATS",
    "VERSION_US=1",
    "_MIPS_SZLONG=32",
    "F3DEX_GBI",
    "LANGUAGE_C",
    "_LANGUAGE_C",
    "BUILD_VERSION=VERSION_H",
    "NDEBUG",
    # newlib's <sys/cdefs.h> (KallistiOS's C library) defines these, and
    # src/audio/libwav.h and sndwav.h open with them; picolibc's does not, and
    # RXDK searches its SDK headers before the project's, so a wrapper header
    # cannot add them. Every game and platform source is C: both are empty.
    "__BEGIN_DECLS=",
    "__END_DECLS=",
]

# Platform/xbox first: its kos.h, dc/*, arch/* stand in for KallistiOS.
# The Makefile's IINC also names bin/us.rev1 and lib/ultralib/include{,/PR,/ido};
# none of those folders exists in the Dreamcast tree (GCC skips a missing -I
# silently, RXDK refuses one), so they are left out.
INCLUDE_PATHS = [
    "Platform/xbox",
    "include",
    ".",
    "src/audio",
]

COMPILE_FLAGS = [
    # RXDK compiles C as -std=c23 (bool/true/false are keywords there); the
    # Dreamcast build is GCC 14's default dialect.
    "-std=gnu17",
    # -fms-compatibility (RXDK's) switches clang to MSVC bitfield layout,
    # which changes the size of a struct mixing bitfield types. The game's
    # structs (and KOS's PVR header words) need GCC's.
    "-mno-ms-bitfields",
    # Single-precision float maths in SSE, as the SH-4 does it in its FPU
    # (the P3 has SSE but not SSE2, so doubles stay on x87).
    "-msse", "-mfpmath=sse",
    "-ffp-contract=off",
    "-fno-strict-aliasing",
    "-fwrapv",
    # The Dreamcast Makefile's -Wno-* set; clang makes several of these
    # errors where GCC 14 warns.
    "-Wno-int-conversion",
    "-Wno-incompatible-pointer-types",
    "-Wno-error=implicit-function-declaration",
    "-Wno-implicit-function-declaration",
    "-Wno-unused-variable",
    "-Wno-parentheses",
    "-Wno-missing-braces",
    "-Wno-switch",
    "-Wno-unknown-pragmas",
    "-Wno-unknown-warning-option",
    "-Wno-pragma-pack",
    "-Wno-microsoft-anon-tag",
    "-Wno-macro-redefined",
    "-Wno-int-to-pointer-cast",
    "-Wno-pointer-to-int-cast",
    # i686-pc-windows-gnu predefines these; nothing in this game is Win32.
    "-U_WIN32", "-U_WIN64", "-U__MINGW32__",
]

# Assets only: keep every array in .data, in source order, as the Dreamcast's
# ASSET_OPTFLAGS do (-fno-common -fno-toplevel-reorder
# -fno-zero-initialized-in-bss).
ASSET_FLAGS = ["-fno-common", "-fno-zero-initialized-in-bss", "-O0"]


def _split_cmdline(line):
    """A command line as Rxdk.Cli echoes it, into arguments (Windows rules,
    as far as its echo needs: quotes group, and are dropped)."""
    args, cur, quoted, have = [], [], False, False
    for ch in line:
        if ch == '"':
            quoted = not quoted
            have = True
        elif ch in " \t" and not quoted:
            if have:
                args.append("".join(cur))
            cur, have = [], False
        else:
            cur.append(ch)
            have = True
    if have:
        args.append("".join(cur))
    return args


_probe_lock = threading.Lock()
_probe_cache = []


def rxdk_compile_prefix():
    """The compiler and the flags the INSTALLED RXDK compiles C with, read
    off its own build of a one-line probe (Rxdk.Cli build --compile-only
    echoes each compile as "$ <command>"): everything in front of its "-x c
    -c <source>". RXDK versions differ -- in where their clang lives, and in
    what their SDK headers demand (1.3's picolibc.h stops a compile without
    -mlong-double-64) -- so the asset segments are compiled with whatever
    this RXDK compiles the game with, not with a copy of one version's
    flags. None when the probe fails (rxdk_base_flags() is used then)."""
    with _probe_lock:
        if _probe_cache:
            return _probe_cache[0]
        prefix = None
        cli = rxdk_cli()
        tmp = tempfile.mkdtemp(prefix="sf64x_rxdk_probe")
        try:
            if os.path.isfile(cli):
                with open(os.path.join(tmp, "probe.c"), "w", encoding="utf-8") as f:
                    f.write("int sf64x_rxdk_probe;\n")
                with open(os.path.join(tmp, "rxdk.project.json"), "w", encoding="utf-8") as f:
                    f.write('{ "name": "probe", "defaultConfiguration": "Release", "configurations": '
                            '{ "Release": { "configuration": "release", "sources": ["probe.c"], '
                            '"libraries": [] } } }\n')
                r = subprocess.run([cli, "build", "--project-root", tmp, "--configuration", "Release",
                                    "--compile-only"], capture_output=True, text=True,
                                   encoding="utf-8", errors="replace")
                for line in (r.stdout + "\n" + r.stderr).splitlines():
                    line = line.strip()
                    if not line.startswith("$ ") or "probe.c" not in line:
                        continue
                    args = _split_cmdline(line[2:])
                    if "-x" in args and args and os.path.isfile(args[0]):
                        prefix = args[:args.index("-x")]
                    break
        except OSError:
            prefix = None
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
        _probe_cache.append(prefix)
        return prefix


def compile_command(src, obj, extra=()):
    cmd = rxdk_compile_prefix()
    if cmd is not None:
        cmd = list(cmd)
    else:
        cc = c_compiler()
        if cc is None:
            raise SystemExit("no Xbox C compiler: the RXDK's clang (%s) or Zig %s "
                             "(RXDK > RXDK Window > Install Prerequisites)" % (clang(), ZIG_VERSION))
        cmd = cc + rxdk_base_flags()
    for p in INCLUDE_PATHS:
        cmd += ["-I", os.path.join(ROOT, p)]
    cmd += ["-D" + d for d in DEFINES]
    cmd += COMPILE_FLAGS + list(extra)
    cmd += ["-x", "c", "-c", src, "-o", obj]
    return cmd
