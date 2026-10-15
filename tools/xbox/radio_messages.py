#!/usr/bin/env python3
"""src/dc/ast_radio.c, the Dreamcast port's radio messages, from the ROM.

    python tools/xbox/radio_messages.py

The Dreamcast port compiles its own copy of the game's message table
(src/dc/ast_radio.c, linked in place of the extracted one) with the
on-screen button names changed to the Dreamcast pad's -- the same as the
Xbox pad's -- in the training mode and tutorial messages. That table is the
game's text, so it is not committed here: it is Torch's extraction from your
ROM (src/assets/ast_radio/ast_radio.c, step 3 of setup.py) with the Dreamcast
port's edits applied, each a line or two, below. Each line to replace must be
found exactly once, or this stops: a different Torch revision would show up
here rather than as a wrong message in the game.
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(ROOT, "src", "assets", "ast_radio", "ast_radio.c")
DST = os.path.join(ROOT, "src", "dc", "ast_radio.c")

# (lines in the extraction, lines in the Dreamcast port), in file order
EDITS = [
    (('    LPR, _Z, SPC, _o, _r, SPC, _R, SPC, _t, _w, _i, _c, _e, RPR, EXM, NWL, ',),
     ('//    LPR, _Z, SPC, _o, _r, SPC, _R, SPC, _t, _w, _i, _c, _e, RPR, EXM, NWL, ', '    LPR, _L, SPC, _o, _r, SPC, _R, SPC, _t, _w, _i, _c, _e, RPR, EXM, NWL, ')),
    (('    LPR, _p, _r, _e, _s, _s, SPC, _t, _h, _e, SPC, _C, ADN, SPC, CDN, NWL, ',),
     ('    LPR, _p, _r, _e, _s, _s, SPC, _t, _h, _e, NWL, _B, SPC, //_C, ADN, SPC, CDN, NWL, ',)),
    (('    LPR, _Z, SPC, _a, _n, _d, SPC, _R, RPR, PRD, NWL, ',),
     ('    LPR, _L, SPC, _a, _n, _d, SPC, _R, RPR, PRD, NWL, ',)),
    (('    _c, _h, _a, _s, _e, SPC, LPR, _C, ALF, SPC, CLF, RPR, PRD, NWL, ',),
     ('    _c, _h, _a, _s, _e, SPC, LPR, _Y, RPR, PRD, NWL, //_C, ALF, SPC, CLF, RPR, PRD, NWL, ',)),
    (('    LPR, ADN, SPC, _a, _n, _d, SPC, _C, ALF, SPC, CLF, RPR, PRD, NWL, ',),
     ('    LPR, ADN, SPC, _a, _n, _d, SPC, _Y, RPR, PRD, NWL, //_C, ALF, SPC, CLF, RPR, PRD, NWL, ',)),
    (('    LPR, ADN, SPC, _a, _n, _d, SPC, _C, ADN, SPC, CDN, RPR, PRD, NWL, ',),
     ('//    LPR, ADN, SPC, _a, _n, _d, SPC, _C, ADN, SPC, CDN, RPR, PRD, NWL, ', '    LPR, ADN, SPC, _a, _n, _d, SPC, _B, RPR, PRD, NWL, // ADN, SPC, CDN, RPR, PRD, NWL, ')),
    (('    LPR, _B, SPC, _B, _u, _t, _t, _o, _n, RPR, PRD, NWL, ',),
     ('//    LPR, _B, SPC, _B, _u, _t, _t, _o, _n, RPR, PRD, NWL, ', '    LPR, _X, SPC, _B, _u, _t, _t, _o, _n, RPR, PRD, NWL, ')),
    (('    _g, _e, _t, SPC, _t, _h, _r, _u, SPC, LPR, _C, ALF, SPC, CLF, RPR, PRD, NWL, ',),
     ('//    _g, _e, _t, SPC, _t, _h, _r, _u, SPC, LPR, _C, ALF, SPC, CLF, RPR, PRD, NWL, ', '    _g, _e, _t, SPC, _t, _h, _r, _o, _u, _g, _h, SPC, LPR, _Y, RPR, PRD, NWL,//_C, ALF, SPC, CLF, RPR, PRD, NWL, ')),
    (('    _f, _r, _o, _m, SPC, _R, _O, _B, _6, _4, SPC, LPR, _C, ART, SPC, CRT, RPR, PRD, NWL, ',),
     ('    _f, _r, _o, _m, SPC, _R, _O, _B, _6, _4, SPC, LPR, CRT, SPC, ART, RPR, PRD, NWL, ',)),
    (('    PRI2, _P, _r, _e, _s, _s, SPC, _C, ALF, SPC, CLF, SPC, _t, _o, NWL, ',),
     ('    PRI2, _P, _r, _e, _s, _s, SPC, _Y, SPC, _t, _o, NWL,// _C, ALF, SPC, CLF, SPC, _t, _o, NWL, ',)),
    (('    PRI2, _P, _r, _e, _s, _s, SPC, ADN, SPC, _a, _n, _d, SPC, _C, ALF, SPC, CLF, SPC, NWL, ',),
     ('    PRI2, _P, _r, _e, _s, _s, SPC, ADN, SPC, _a, _n, _d, SPC, _Y, SPC, NWL, //_C, ALF, SPC, CLF, SPC, NWL, ',)),
    (('    _p, _r, _e, _s, _s, SPC, ADN, SPC, _a, _n, _d, SPC, _C, ADN, SPC, CDN, PRD, NWL, ',),
     ('    _p, _r, _e, _s, _s, SPC, ADN, SPC, _a, _n, _d, SPC, _B, PRD, /* _C, ADN, SPC, CDN, PRD, */ NWL, ',)),
    (('    PRI2, _P, _r, _e, _s, _s, SPC, _B, SPC, _t, _o, SPC, _b, _o, _m, _b, PRD, NWL, ',),
     ('//    PRI2, _P, _r, _e, _s, _s, SPC, _B, SPC, _t, _o, SPC, _b, _o, _m, _b, PRD, NWL, ', '    PRI2, _P, _r, _e, _s, _s, SPC, _X, SPC, _t, _o, SPC, _b, _o, _m, _b, PRD, NWL, ')),
    (('    _p, _r, _e, _s, _s, SPC, _Z, SPC, _o, _r, SPC, _R, SPC, _t, _w, _i, _c, _e, PRD, NWL, ',),
     ('//    _p, _r, _e, _s, _s, SPC, _Z, SPC, _o, _r, SPC, _R, SPC, _t, _w, _i, _c, _e, PRD, NWL, ', '    _p, _r, _e, _s, _s, SPC, _L, SPC, _o, _r, SPC, _R, SPC, _t, _w, _i, _c, _e, PRD, NWL, ')),
    (('    PRI2, _P, _r, _e, _s, _s, SPC, _C, ADN, SPC, CDN, NWL, ',),
     ('    PRI2, _P, _r, _e, _s, _s, SPC, _B, SPC, _B, _u, _t, _t, _o, _n, NWL,//_C, ADN, SPC, CDN, NWL, ',)),
    (('    _p, _r, _e, _s, _s, SPC, _Z, SPC, _a, _n, _d, SPC, _R, PRD, NWL, ',),
     ('    _p, _r, _e, _s, _s, SPC, _L, SPC, _a, _n, _d, SPC, _R, PRD, NWL, ',)),
    (('    PRI1, _P, _r, _e, _s, _s, SPC, _Z, SPC, _o, _r, SPC, _R, SPC, _t, _o, NWL, ',),
     ('//    PRI1, _P, _r, _e, _s, _s, SPC, _Z, SPC, _o, _r, SPC, _R, SPC, _t, _o, NWL, ', '    PRI1, _P, _r, _e, _s, _s, SPC, _L, SPC, _o, _r, SPC, _R, SPC, _t, _o, NWL, ')),
    (('    PRI1, _P, _r, _e, _s, _s, SPC, _Z, SPC, _o, _r, SPC, _R, SPC, _t, _w, _i, _c, _e, NWL, ',),
     ('//    PRI1, _P, _r, _e, _s, _s, SPC, _Z, SPC, _o, _r, SPC, _R, SPC, _t, _w, _i, _c, _e, NWL, ', '    PRI1, _P, _r, _e, _s, _s, SPC, _L, SPC, _o, _r, SPC, _R, SPC, _t, _w, _i, _c, _e, NWL, ')),
    (('    PRI1, _P, _r, _e, _s, _s, SPC, _B, SPC, _t, _o, SPC, _b, _o, _m, _b, PRD, NWL, ',),
     ('//    PRI1, _P, _r, _e, _s, _s, SPC, _B, SPC, _t, _o, SPC, _b, _o, _m, _b, PRD, NWL, ', '    PRI1, _P, _r, _e, _s, _s, SPC, _X, SPC, _t, _o, SPC, _b, _o, _m, _b, PRD, NWL, ')),
    (('    PRI1, _P, _r, _e, _s, _s, SPC, _C, ALF, SPC, CLF, SPC, _f, _o, _r, NWL, ',),
     ('    PRI1, _P, _r, _e, _s, _s, SPC, _Y, SPC, _f, _o, _r, NWL, //_C, ALF, SPC, CLF, SPC, _f, _o, _r, NWL, ',)),
    (('    PRI1, _P, _r, _e, _s, _s, SPC, _C, ADN, SPC, CDN, SPC, _t, _o, SPC, NWL, ',),
     ('    PRI1, _P, _r, _e, _s, _s, SPC, _B, SPC, _t, _o, NWL, //_C, ADN, SPC, CDN, SPC, _t, _o, SPC, NWL, ',)),
    (('    _p, _r, _e, _s, _s, SPC, ADN, SPC, _a, _n, _d, SPC, _C, ALF, SPC, CLF, PRD, NWL, ',),
     ('    _p, _r, _e, _s, _s, SPC, ADN, SPC, _a, _n, _d, SPC, _Y, PRD, NWL,//_C, ALF, SPC, CLF, PRD, NWL, ',)),
    (('    PRI1, _P, _r, _e, _s, _s, SPC, ADN, SPC, _a, _n, _d, SPC, _C, ADN, SPC, CDN, NWL, ',),
     ('    PRI1, _P, _r, _e, _s, _s, SPC, ADN, SPC, _a, _n, _d, SPC, _B, NWL, //_C, ADN, SPC, CDN, NWL, ',)),
    (('    PRI1, _P, _r, _e, _s, _s, SPC, _C, AUP, SPC, CUP, SPC, _t, _o, SPC, NWL, ',),
     ('    PRI1, _P, _r, _e, _s, _s, SPC, CUP, SPC, /* _C,  */AUP, /* SPC, CUP, */ SPC, _t, _o, SPC, NWL, ',)),
    (('    _m, _e, _s, _s, _a, _g, _e, _s, CMA, SPC, _p, _r, _e, _s, _s, NWL, ', '    _C, ART, SPC, CRT, PRD, NWL, '),
     ('    _m, _e, _s, _s, _a, _g, _e, _s, CMA, SPC, _p, _r, _e, _s, _s, NWL, CRT, SPC,', '    /* _C,  */ART, /* SPC, CRT,  */PRD, NWL, ')),
]


def main():
    if not os.path.isfile(SRC):
        sys.exit("radio_messages: %s is missing (Torch's extraction, setup.py step 3)" % SRC)
    with open(SRC, encoding="utf-8", newline="") as f:
        lines = f.read().replace("\r\n", "\n").split("\n")
    for old, new in EDITS:
        old, new = list(old), list(new)
        at = [k for k in range(len(lines) - len(old) + 1) if lines[k:k + len(old)] == old]
        if len(at) != 1:
            sys.exit("radio_messages: %d matches for %r (expected 1)" % (len(at), old[0]))
        lines[at[0]:at[0] + len(old)] = new
    os.makedirs(os.path.dirname(DST), exist_ok=True)
    tmp = DST + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    os.replace(tmp, DST)
    print("radio_messages: %s (%d edits)" % (os.path.relpath(DST, ROOT), len(EDITS)))


if __name__ == "__main__":
    main()
