# Contributing

Bug fixes and improvements are welcome. Say in the pull request where you saw
the change working: a console (which one, which video mode) or xemu, and the
level.

## Building

`README.md` has the setup. In short: the RXDK, Python 3, git, CMake and Visual
Studio's C++ tools, and your US v1.1 ROM as `baserom.us.rev1.z64`, then

```bash
python setup.py --check     # what is there and what is missing
python setup.py             # ROM -> out/Release/XISO/Star-Fox-64-X.iso
python tools/xbox/cc_check.py --implicit   # every compile error at once
```

How the build and the port fit together is in `docs/TECHNICAL.md`.

## How the code is written

- The Xbox platform layer is `Platform/xbox/`: KallistiOS's API on the Xbox,
  with KOS's own names, types and behaviour (v2.2.1).
- The Dreamcast port's files (`src/`, `include/`, `tools/`) change only where
  the Xbox needs it. Every change is inside `#if defined(TARGET_XBOX)` and
  carries an `XBOX:` comment saying why.
- An option goes in `Platform/xbox/xbox_settings.h` and `.c` (its default is
  the game as it shipped) and in the menu table in `xbox_menu.c`.
- Never print every frame: the debug channel blocks once its buffer fills,
  which stalls the game.
- No ROM-derived data in the repository. Anything taken from the ROM is made
  by `setup.py` on the builder's machine and listed in `.gitignore`;
  `python tools/xbox/release_check.py` checks the tree.

## License

By contributing you agree that your contribution is under the MIT License in
`LICENSE` (changes to the CC0 parts stay CC0 if you prefer; say so).
