#!/usr/bin/env python3
"""Compile every source in rxdk.project.json in parallel and report ALL errors.

Rxdk.Cli builds one file at a time and stops at the first failure: right for
a build, slow for bringing up a port. This runs the same clang with the same
command line (tools/xbox/xbconfig.py, which the manifest is written from)
across every core and prints a per-file summary. -fsyntax-only unless --obj.

  python tools/xbox/cc_check.py                    # all files
  python tools/xbox/cc_check.py src/audio/libwav.c # just these
  python tools/xbox/cc_check.py --errors 3         # first 3 errors per file
  python tools/xbox/cc_check.py --implicit         # also report implicit declarations
"""
import argparse
import concurrent.futures as cf
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xbconfig  # noqa: E402

ROOT = xbconfig.ROOT


def run(src, nerr, implicit, obj):
    extra = []
    if implicit:
        extra = ["-Wimplicit-function-declaration"]
    cmd = xbconfig.compile_command(os.path.join(ROOT, src), obj or "NUL", extra)
    if not obj:
        i = cmd.index("-o")
        del cmd[i:i + 2]
        cmd.append("-fsyntax-only")
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    out = r.stdout + r.stderr
    errs = [l for l in out.splitlines() if re.search(r"\berror:", l)]
    impl = [l for l in out.splitlines() if "undeclared function" in l or "implicitly declar" in l]
    return src, r.returncode, errs[:nerr] if nerr else errs, len(errs), impl, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="*")
    ap.add_argument("--errors", type=int, default=0)
    ap.add_argument("--implicit", action="store_true")
    ap.add_argument("--full", action="store_true", help="print the whole compiler output")
    a = ap.parse_args()
    m = json.load(open(os.path.join(ROOT, "rxdk.project.json"), encoding="utf-8"))
    files = a.files or m["configurations"]["Release"]["sources"]
    bad = 0
    with cf.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        for src, rc, errs, n, impl, out in ex.map(lambda f: run(f, a.errors, a.implicit, None), files):
            if rc or (a.implicit and impl):
                bad += bool(rc)
                print("== %s: %d errors" % (src, n))
                if a.full:
                    print(out)
                else:
                    for e in errs:
                        print("   " + e.replace(ROOT + os.sep, ""))
                    for e in sorted(set(impl)):
                        print("   " + e.replace(ROOT + os.sep, ""))
    print("cc_check: %d of %d files failed" % (bad, len(files)))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
