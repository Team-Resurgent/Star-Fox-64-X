#!/usr/bin/env python3
"""Check that a tree holds no game data before it is published.

    python tools/xbox/release_check.py [root]

The files checked are the ones a commit would take: `git ls-files` (tracked,
and untracked but not ignored) in a git checkout, every file otherwise. It
fails, listing each, on:

  * a ROM, disc image or build output by name (.z64/.n64/.v64/.u64, .iso,
    .xbe, objects, libraries, executables);
  * anything under a folder setup.py fills from the ROM or the build (data/,
    baserom/, src/assets/, include/assets/, Platform/xbox/gen/, build/, out/,
    tools/Torch/) or a file it writes from the ROM (the AICA sample table,
    the radio messages);
  * an N64 ROM header, in any byte order, at the start of any file;
  * a file over 1 MiB (the largest source file is a fraction of that; a
    big file is almost always data);
  * a path on someone's own machine in a text file (a user profile, a home
    folder, a Desktop): the build finds everything from where it is run.

Exit status 0 when clean, 1 otherwise.
"""
import os
import re
import subprocess
import sys

ROM_EXT = (".z64", ".n64", ".v64", ".u64", ".iso", ".xbe", ".xbx", ".obj", ".o", ".lib", ".a",
           ".exe", ".dll", ".so", ".pdb", ".elf", ".cdi")
GENERATED_DIRS = ("data/", "baserom/", "src/assets/", "include/assets/", "srcjp/assets/",
                  "Platform/xbox/gen/", "build/", "out/", "tools/Torch/", "Xbox/", "Star-Fox-64-X/",
                  ".vs/", "sf_data/")
GENERATED_FILES = ("src/audio/aica_sample_table.c", "src/audio/aica_sample_table.h", "src/dc/ast_radio.c",
                   "torch.hash.yml")
ROM_MAGIC = (b"\x80\x37\x12\x40", b"\x37\x80\x40\x12", b"\x40\x12\x37\x80", b"\x12\x40\x80\x37")
MAX_SIZE = 1024 * 1024
BINARY_EXT = (".png", ".ico", ".bin", ".jpg", ".gif")
# a path that only exists on the machine it was written on: a Windows user
# profile (a drive's Users or Documents and Settings folder), a Unix home, a
# Desktop folder
PERSONAL_PATH = re.compile(r"[A-Za-z]:[\\/]{1,2}(?:Users|Documents and Settings)[\\/]{1,2}[^\\/\s\"']+"
                           r"|/(?:home|Users)/[A-Za-z0-9_.-]+/"
                           r"|[\\/]Desktop[\\/]", re.IGNORECASE)


def candidate_files(root):
    if os.path.exists(os.path.join(root, ".git")):
        r = subprocess.run(["git", "-C", root, "ls-files", "-co", "--exclude-standard", "-z"],
                           capture_output=True)
        if r.returncode == 0:
            names = [n.decode("utf-8") for n in r.stdout.split(b"\0") if n]
            # submodule entries are folders, not files
            return [n for n in names if os.path.isfile(os.path.join(root, n))]
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d != ".git"]
        for f in filenames:
            out.append(os.path.relpath(os.path.join(dirpath, f), root).replace(os.sep, "/"))
    return out


def problems(root, rel):
    low = rel.lower()
    p = []
    if low.endswith(ROM_EXT):
        p.append("ROM, disc image or build output")
    if any(rel.startswith(d) for d in GENERATED_DIRS):
        p.append("in a folder setup.py generates")
    if rel in GENERATED_FILES:
        p.append("generated from the ROM by setup.py")
    path = os.path.join(root, rel)
    try:
        size = os.path.getsize(path)
        with open(path, "rb") as f:
            head = f.read(4)
    except OSError as e:
        return ["unreadable: %s" % e]
    if head in ROM_MAGIC:
        p.append("starts with an N64 ROM header")
    if size > MAX_SIZE:
        p.append("%.1f MiB (over 1 MiB)" % (size / 1048576.0))
    elif not low.endswith(BINARY_EXT):
        with open(path, "rb") as f:
            text = f.read().decode("utf-8", errors="replace")
        m = PERSONAL_PATH.search(text)
        if m:
            line = text.count("\n", 0, m.start()) + 1
            p.append("a path on someone's own machine, line %d: %s" % (line, m.group(0)))
    return p


def main():
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                           os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    files = candidate_files(root)
    bad = []
    for rel in sorted(files):
        p = problems(root, rel)
        if p:
            bad.append((rel, p))
    total = sum(os.path.getsize(os.path.join(root, f)) for f in files)
    if bad:
        print("release_check: %d of %d files must not be published:" % (len(bad), len(files)))
        for rel, p in bad:
            print("  %s -- %s" % (rel, "; ".join(p)))
        return 1
    print("release_check: clean -- %d files, %.1f MiB, no game data" % (len(files), total / 1048576.0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
