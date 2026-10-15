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


def llvm_root():
    return os.path.join(rxdk_root(), "llvm", "xboxog-windows-x64")


def clang():
    return os.path.join(llvm_root(), "bin", "clang.exe")


def clang_resource_include():
    hits = sorted(glob.glob(os.path.join(llvm_root(), "lib", "clang", "*", "include")))
    return hits[-1] if hits else None


def sdk_include():
    return os.path.join(rxdk_root(), "sdk", "include")


# What Rxdk.Cli puts in front of every C compile (its build log, RXDK 1.2.8),
# minus the optimisation level and debug info, which do not change a layout.
def rxdk_base_flags():
    return ["-target", "i686-pc-windows-gnu",
            "-isystem", clang_resource_include(),
            "-fno-sanitize=undefined", "-ffreestanding", "-fno-stack-protector",
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


def compile_command(src, obj, extra=()):
    cmd = [clang()] + rxdk_base_flags()
    for p in INCLUDE_PATHS:
        cmd += ["-I", os.path.join(ROOT, p)]
    cmd += ["-D" + d for d in DEFINES]
    cmd += COMPILE_FLAGS + list(extra)
    cmd += ["-x", "c", "-c", src, "-o", obj]
    return cmd
