# Star Fox 64 X -- technical notes

For people working on the port. Building and playing are in the
[README](../README.md).

## The build

`setup.py` runs, in order,
and every time (nothing is skipped because its output already exists):

1. checks the ROM and decompresses it (`tools/xbox/decompress_rom.py`, the
   Makefile's `decompress` in Python);
2. fetches Torch into `tools/Torch` at the pinned revision and builds it
   (CMake, incremental);
3. extracts the assets as the Makefile's `assets` rule does: Torch's `code`,
   `header` and `modding export`, then `tools/strip_dc_syncs.py`, and makes
   the Dreamcast port's radio messages (the extracted ones with its
   button-name edits, `tools/xbox/radio_messages.py`);
4. makes the audio as its `sf-data` rule does: the Yamaha-ADPCM sample pool
   and table (`tools/aica/sf64_emit.py`), the sequence and bank files;
5. makes the Xbox pieces: the asset segments (`tools/xbox/build_assets.py`,
   standing in for the Dreamcast's `sh-elf-ld`/`objcopy` step), the dashboard
   icons (Fox's radio portrait, from the ROM), `rxdk.project.json`
   (`gen_manifest.py`, from the Makefile's own object lists) and the Visual
   Studio project (`gen_vcxproj.py`);
6. builds with `Rxdk.Cli`.

`python tools/xbox/cc_check.py` compiles every source in parallel and lists
every error at once (`--implicit` also lists calls to undeclared functions).

## Display features

**60 fps** keeps the game's logic at its own 30 fps and draws each logic
frame once per vertical blank it lasts: the extra draws blend every object's
matrix (and the starfield) between the last logic frame and this one, as
Starship does (`Platform/xbox/xbox_interp.c`). The picture is one logic frame
behind; HUD, fades and texture animation stay at 30. When the frame and its
extra draws would not fit in the time, it drops back to one draw a frame for a
second before trying again; the frame-time readout shows the draws per frame.

The renderer's square roots (a 1/w for every vertex) and the game's use one
SSE instruction: the SDK libc's `sqrtf` is a bit-at-a-time software loop, and
RXDK's `-fno-builtin` sends every call to it (`Platform/xbox/sh4zam_x86.c`).

Widescreen widens the 3D view, the starfield, full-screen fades and the
game's own culling. With "HUD at screen edges" the radar, boost gauge, bombs
and lives go to the right edge and the shield, gold rings, hit count and boss
gauge to the left, each the same distance from the edge as on the 4:3 screen
(Starship's HUD aspect "Expand"); the edge arrows and HELP!! markers follow.
The 2D parts move through the renderer's rectangle offset, the parts the HUD
places in 3D by the same distance at their depth (`XboxHud_*` in
`Platform/xbox/xbox_enhance.c`). The radio box follows on the title, and in a
level with its own option. Menus and 2D screens stay 4:3 in the middle.

In versus, each split-screen pane takes the 16:9 picture's extra width on
its screen side: its camera is centred in its half of the wide screen and its
2D (radar, gauges, portrait, the CAMERA box) moves with it, so the left and
right players see the same layout. With 60 fps, each
pane's objects and starfield are matched with the same pane's last frame.

The Options sound test's spectrum analyzer works here (it is fed from the
Xbox's software mix), as it does on the N64 and did not on the Dreamcast.

## How it maps onto the Xbox

| Dreamcast | Xbox (`Platform/xbox/`) |
| --- | --- |
| PowerVR (KOS `pvr_*`, the port's raw-TA backend) | `pvr_xbox.c`: Direct3D 8 on the NV2A, with the PVR's 8 MB of texture memory kept as it is and its polygon headers decoded per batch. |
| AICA (KOS `snd_*`, `spu_*`, `g2_*`, the stock ARM driver) | `aica/`: sound RAM, the channel registers and command queue, KallistiOS's stock ARM firmware compiled for the x86, 64 channels mixed in software into a DirectSound stream. `kos_snd_stream_xbox.c`: KOS's sound streams. |
| SH-4 MMU segment mapping | `xbox_segments.c`: the N64 segments committed at their own addresses (`0x0S000000`). |
| SH4ZAM, SH-4 `fsca`/`fsrra` | `sh4zam_x86.h`, `dc/fmath.h`: the same results on SSE, angles quantised as `fsca` quantises them. |
| maple bus | `kos_maple_xbox.c`: XInput pads, their rumble motors, KOS's attach/detach callbacks. |
| VMU | `kos_vmu_xbox.c`, `kos_vmu_pkg.c`, `kos_fs_xbox.c`: a VMU whose files live in the title's save game on `U:`. |
| `/cd`, `/pc` | `kos_fs_xbox.c`: `D:\data`. |
| threads, genwait, worker threads, one-shot timers, vblank, the watchdog timer | `kos_thread_xbox.c`, `kos_extra_xbox.c`, `kos_wdt_xbox.c`, `kos_sys_xbox.c` |
| debug log | `kos_log_xbox.c`: to xbWatson through a ring, never blocking a game thread. |

The KallistiOS behaviour reproduced is that of **v2.2.1**, the version the
Dreamcast port is built against.

The changes to the Dreamcast port's own files are each marked `XBOX:`:

* `src/sys/sys_main.c` -- the segments set up by `xbox_segments.c` instead of
  the SH-4 TLB, and `D:/data` as the data root;
* `src/dmatable.c` -- the segment table without the SH-4 page-mapping arrays;
* `include/sh4zam.h` -- the x86 SH4ZAM;
* `src/audio/mixer.c` -- C versions of three SH-4 assembler helpers, and no
  SH-4 register bindings;
* `src/gfx/gfx_retro_dc.c`, `src/gfx/gfx_pvr.c` -- the TA store-queue copy and
  one raw PVR register write through the PVR layer;
* `include/sf64audio_provisional.h`, `src/audio/audio_playback.c` -- the
  "valid pointer" floor is the Xbox's, not the Dreamcast's `0x8C010000`;
* `src/audio/audio_load.c` -- the watchdog-timer self test waits for the
  timer's first tick (the Dreamcast relies on a heap clear outlasting it);
* `src/gfx/gfx_buf.c`, `src/gfx/gfx_retro_dc.c`, `src/audio/mixer.c` -- 8 KB
  buffer alignment (COFF's limit) for the SH-4's 16 KB;
* `src/sys/sys_math.c`, `src/ultra_reimpl.c` -- `osGetTime` without a
  qualifier clang rejects;
* `src/overlays/ovl_menu/fox_title.c`, `fox_option.c` -- the N64 logo and
  console art from the ROM in place of the Dreamcast's.
* `src/overlays/ovl_menu/fox_title.c`, `src/engine/fox_versus.c` -- the Xbox
  pad's buttons in place of the Dreamcast's: "Press START" without the
  Dreamcast's triangle start button, and the versus menu's A and X in the
  Xbox's green and blue (`Platform/xbox/xbox_*.inc.c`, made from the
  Dreamcast port's own art by `tools/xbox/make_xbox_buttons.py`).

Fixes beyond the Dreamcast port, also marked `XBOX:`:

* the Options sound test's spectrum analyzer: the decomp's matched Hartley
  transform (`audio_synthesis.c`, sonicdcer/sf64 `ed63cd69`) fed from the
  Xbox's software mix (`audio_thread.c`);
* `audio_heap.c` -- the sample-cache discard checks each cache entry
  against the font's banks (sonicdcer/sf64 `153ab744`);
* `fox_end1.c` -- the ending's state reset for every ending, as the N64's
  overlay load did, so a second playthrough's ending plays right (Starship);
* `fox_demo.c` and the Area 6, Meteo, Sector Y, Andross and ending overlays
  -- the rest of Starship's N64 cutscene timings, which keep the Warp Zone,
  Solar intro, Gorgon, MeCrusher, Shogun, Andross and ending cutscenes in
  step with their music;
* `fox_demo.c` -- no white flash in the Sector Y intro; `fox_bo.c` -- Bolse's
  Star Wolf lines no longer cut each other off (Starship).

The options menu's features are game-code changes in the same style, each
behind its setting, and `Platform/xbox/xbox_menu.c`, `xbox_settings.c`,
`xbox_enhance.c` and `xbox_levelselect.inc`.

`src/sys/sys_fault.c` (the N64 crash screen, never called) is not built: the
Dreamcast link discards it with `--gc-sections`, RXDK's link has no such step.

## Layout

```
setup.py                      the one-command build
rxdk.project.json             the build manifest (tools/xbox/gen_manifest.py)
Star-Fox-64-X.sln/.vcxproj    Visual Studio (tools/xbox/gen_vcxproj.py)
Platform/xbox/                the Xbox platform layer (KallistiOS on the Xbox)
tools/xbox/                   host-side build glue
src/, include/                the Dreamcast port (game code and its KOS-facing layer)
data/                         the game data (generated, not committed)
```

## Publishing

Everything `setup.py` makes from the ROM is in `.gitignore`.
`python tools/xbox/release_check.py` checks a tree before it is pushed: no ROM,
disc image, build output or generated asset, no file starting with an N64 ROM
header, nothing over 1 MiB. `python tools/xbox/stage_release.py [folder]`
copies a working copy's publishable files into a clean folder (keeping that
folder's own `.git`) and runs the check on it.
