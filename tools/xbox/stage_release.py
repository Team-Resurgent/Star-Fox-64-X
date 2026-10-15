#!/usr/bin/env python3
"""Stage the files a GitHub release of the Xbox port holds into a clean folder.

    python tools/xbox/stage_release.py [dest]     # default: ../Star-Fox-64-X-github

The working copy is a checkout of the Dreamcast port (sf64-dc) with the Xbox
port in it, the ROM beside it and everything setup.py made from the ROM.
This copies out exactly what a commit should hold:

  * every file git tracks or would add (`git ls-files -co --exclude-standard`),
    as it is in the working tree;
  * less tracked files the .gitignore now names (the ROM-derived AICA sample
    table the Dreamcast repository commits; setup.py writes it every build);
  * less the Dreamcast repository's submodules (.gitmodules and the folders
    it names): Torch, the one the Xbox build uses, is fetched by setup.py at
    its pinned revision; the others are decomp-matching tools it never runs.

The destination is emptied first, except for its .git folder, so a repository
kept there sees each staging as an ordinary change. Then release_check.py
runs over the result. Nothing is committed or pushed.
"""
import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
LEAVE_OUT = {".gitmodules"}


def git(*args):
    r = subprocess.run(["git", "-C", ROOT] + list(args), capture_output=True)
    if r.returncode != 0:
        sys.exit("stage_release: git %s failed: %s" % (" ".join(args), r.stderr.decode(errors="replace")))
    return [n.decode("utf-8") for n in r.stdout.split(b"\0") if n]


def main():
    dest = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                           os.path.join(os.path.dirname(ROOT), os.path.basename(ROOT) + "-github"))
    if os.path.normcase(dest) == os.path.normcase(ROOT) or os.path.normcase(dest).startswith(os.path.normcase(ROOT + os.sep)):
        sys.exit("stage_release: the destination must be outside the working copy")

    files = set(git("ls-files", "-co", "--exclude-standard", "-z"))
    files -= set(git("ls-files", "-ci", "--exclude-standard", "-z"))   # tracked, now ignored
    files -= LEAVE_OUT
    files = sorted(f for f in files if os.path.isfile(os.path.join(ROOT, f)))   # drops submodule entries

    if os.path.isdir(dest):
        for name in os.listdir(dest):
            if name == ".git":
                continue
            p = os.path.join(dest, name)
            if os.path.isdir(p) and not os.path.islink(p):
                shutil.rmtree(p)
            else:
                os.remove(p)
    os.makedirs(dest, exist_ok=True)

    total = 0
    for f in files:
        src = os.path.join(ROOT, f)
        dst = os.path.join(dest, f)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
        total += os.path.getsize(src)
    print("stage_release: %d files, %.1f MiB -> %s" % (len(files), total / 1048576.0, dest))

    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "xbox", "release_check.py"), dest])
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
