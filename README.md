# Star Fox 64 for the original Xbox

A native original-Xbox port of *Star Fox 64* (N64, 1997), built from
[sf64-dc](https://github.com/jnmartin84/sf64-dc), jnmartin84's Dreamcast port of
the [sf64 decompilation](https://github.com/sonicdcer/sf64), the same way as the
Mario Kart 64, Ocarina of Time and Super Smash Bros. Xbox ports.

**No game data is included.** You need your own Star Fox 64 ROM: the **US v1.1**
release, as a big-endian `.z64` file (MD5 `741a94eee093c4c8684e66b89f8685e8`).
Everything taken from it is generated on your own machine.

## Features

* The whole game: story mode with every route, training, versus for up to four
  players, the ending, saves on the hard disk
* 720p and 480p, with an optional 16:9 widescreen mode (HUD at the screen edges)
* An optional 60 fps mode
* Rumble on all four controller ports
* An in-game options menu (press **BACK**) with cheats, enhancements and
  restorations from [Starship](https://github.com/HarbourMasters/Starship), the
  PC port: level select, infinite lives, invincibility, hyper laser, rapid
  fire, the beta boost gauge and gold coin, and more

## Requirements

* Windows 10 or 11
* the RXDK toolchain: in Visual Studio, **RXDK > RXDK Window > Install
  Prerequisites**
* [Python](https://www.python.org/downloads/) 3.8 or later
* [Git](https://git-scm.com/download/win)
* [Visual Studio](https://visualstudio.microsoft.com/) 2022 or later with
  **Desktop development with C++**, and [CMake](https://cmake.org/download/)
* an internet connection the first time you build (the asset extractor,
  [Torch](https://github.com/HarbourMasters/Torch), is downloaded and built)

## Building

1. Download this repository (**Code > Download ZIP**, or `git clone`).
2. Put your ROM in its folder, named `baserom.z64`.
3. Open a command prompt in that folder and run:

```bash
python setup.py
```

The first build takes a while (it builds Torch); later ones are quicker. The
result is **`out\Release\XISO\Star-Fox-64-X.iso`**.

If something is missing, `python setup.py --check` lists what is installed and
what is not.

### In Visual Studio

Run `python setup.py --skip-build` once, open `Star-Fox-64-X.sln`, choose the
**Xbox** platform and Build. **F5** runs it on your devkit with the Xbox
Debugger (RXDK).

## Running

* **Disc image:** boot `out\Release\XISO\Star-Fox-64-X.iso` on a modded console
  or in [xemu](https://xemu.app/).
* **Hard disk:** copy the folder `out\Release\Build\Star-Fox-64-X\` (the
  `default.xbe` and its `data` folder) to your Xbox, and launch `default.xbe`.

The game saves to the hard disk as a normal save game (dashboard > Memory >
Star Fox 64).

### Video modes

The video mode follows your dashboard settings: **720p** on an HD component
cable with 720p enabled, otherwise **480p** when enabled, otherwise 480i. The
picture is 4:3 with black bars at the sides; turn on **Widescreen** in the
options menu for a full 16:9 picture on a widescreen TV (720p, or 480 with the
dashboard's widescreen setting).

## Controls

| Xbox | | N64 |
| --- | --- | --- |
| Left stick | steer | Control stick |
| A | fire laser (hold to charge) | A |
| X | fire bomb | B |
| Y | boost | C-left |
| B | brake | C-down |
| Down + Y / Down + B | somersault / U-turn | |
| Left / right trigger | tilt; press twice to barrel roll | Z / R |
| D-pad up | change camera | C-up |
| D-pad right | answer ROB | C-right |
| START | pause | START |
| BACK | the options menu | |
| BLACK | charged shot, with the "Charge on BLACK" option | L |
| WHITE | set a level checkpoint here | |
| START + A + B + X + Y | quit to the dashboard | |

All four controller ports work, for versus mode.

## Options menu

Press **BACK** at any time. Up/down choose, left/right or A change, B or BACK
close. In a level the game waits while the menu is open. Your choices are
saved and kept for next time.

* **Display:** widescreen, HUD at the screen edges, radio box at the screen
  edge, 60 fps, translucency sorting, frame-time readout
* **Controls:** invert the Y axis, charged shot on BLACK
* **Audio:** master, music, voice and sound-effect volume
* **Cheats:** infinite lives, invincible, unbreakable wings, infinite bombs,
  infinite boost, hyper laser, rapid fire, laser range, self destruct
  (D-pad down), start with a wingman down, repair wingmen, level select (on
  the map: D-pad to choose, WHITE for the planet's other start), skip
  briefings, unlock expert mode, sound-effect jukebox (WHITE in the sound test)
* **Enhancements:** team faces in cockpits, red radio for enemies, cockpit
  glass opacity, no Gorgon screen flashes, radar outlines, Macbeth camera fix,
  all Sector Z fighters, full detail at Katina, 999 hit cap
* **Restorations:** the beta gold coin, bomb explosion and boost gauge; the
  Japanese Sector Z missile cutscene
* **Game:** return to the title screen, quit to the dashboard, clear a level
  checkpoint

**Level checkpoint:** in a level, press **WHITE** to make the level start from
where you are from then on, the way the game's own mid-level checkpoint does.
Clear it again under **Game > Level checkpoint**.

## Credits

* [sf64](https://github.com/sonicdcer/sf64), the decompilation
* [sf64-dc](https://github.com/jnmartin84/sf64-dc), the Dreamcast port this is built from
* [Starship](https://github.com/HarbourMasters/Starship), the PC port, whose
  fixes, cheats, enhancements and restorations the options menu carries over
* [Torch](https://github.com/HarbourMasters/Torch), the asset extractor
* [KallistiOS](https://github.com/KallistiOS/KallistiOS), the Dreamcast OS whose
  API the Xbox layer reimplements
* Team Resurgent's RXDK

## Licensing

See [LICENSE](LICENSE) and [NOTICE](NOTICE). The decompilation and the
Dreamcast port are CC0; the Xbox port's own code is MIT (Team Resurgent);
code taken from KallistiOS stays under its own license. No ROM, asset or other
game data is in this repository.

Technical notes for developers are in [docs/TECHNICAL.md](docs/TECHNICAL.md);
the Dreamcast port's own README is [README.dreamcast.md](README.dreamcast.md).
