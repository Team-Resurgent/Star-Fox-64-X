/* xbox_settings.h -- the Xbox port's options: cheats, enhancements,
 * restorations and display settings, most of them Starship's
 * (HarbourMasters/Starship), set from the in-game menu (xbox_menu.c, BACK)
 * and kept in the title's save game as sf64x.ini (xbox_settings.c).
 *
 * Game code reads them as XBOX_CFG(name). Every one defaults to the game as
 * it shipped, except team faces, which the Dreamcast port already turns on.
 */
#ifndef XBOX_SETTINGS_H
#define XBOX_SETTINGS_H

/* A level's checkpoint (Starship's developer "Set Checkpoint"): where the
 * level starts from, as the game's own mid-level checkpoint does -- the
 * ground under the player, how far along the path, and how far through the
 * level's object list. One per level number (LEVEL_CORNERIA .. LEVEL_VERSUS). */
#define XBOX_CP_LEVELS 21
typedef struct XboxCheckpoint {
    int set;
    int ground;
    int progress;
    int index;
} XboxCheckpoint;

typedef struct XboxSettings {
    /* display */
    int widescreen;          /* 16:9 picture: the wider view */
    int trSorted;            /* translucent list sorted far to near (PVR) instead of submission order */
    int fps60;               /* 60 fps by drawing blended frames between the game's 30 (xbox_interp.c) */
    int frameTimes;          /* on screen: the frame's game logic and total work, in ms */
    int hudWide;             /* widescreen: HUD at the 16:9 picture's edges (Starship's "Expand") */
    int radioWide;           /* widescreen: in a level, the radio box at the left edge too */

    /* controls */
    int invertY;             /* stick up flies down... the other way round */
    int lToCharge;           /* the charged shot on L (BLACK): Starship's "Hold L to Charge" */

    /* audio */
    int masterVolume;        /* everything the game plays, 0..100 percent */

    /* cheats */
    int infiniteLives;
    int invincible;
    int unbreakableWings;
    int infiniteBombs;
    int infiniteBoost;
    int hyperLaser;
    int rapidFire;           /* hold A: a stream of shots; release to charge */
    int laserRange;          /* percent of a shot's normal life, 15..800 */
    int selfDestruct;        /* D-pad down blows up the Arwing */
    int falcoDead;           /* start each level with this wingman down */
    int slippyDead;
    int peppyDead;
    int repairWingmen;       /* a wingman shot down flies the next mission anyway */
    int levelSelect;         /* map: pick any planet with the D-pad, WHITE for its other start */
    int skipBriefing;        /* with the level select: straight into the level */
    int forceExpert;         /* expert mode and sound test without the medals */
    int sfxJukebox;          /* sound test: WHITE shows every sound effect */

    /* enhancements */
    int teamFaces;           /* wingmen's faces in their cockpits in cutscenes */
    int enemyRedRadio;       /* enemy radio messages in a red box */
    int cockpitOpacity;      /* cockpit glass, 0..255 (the game's own is 120) */
    int disableGorgonFlash;  /* no full-screen flashes from Area 6's boss */
    int fighterOutlines;     /* outlined Arwing and Wolfen marks on the radar */
    int maCameraFix;         /* Macbeth's ending camera, as its code meant it */
    int szActorFix;          /* Sector Z spawns all 12 of its fighters */
    int disableLOD;          /* Katina's fighters always at full detail */
    int hitCap999;           /* hit counter caps at 999, as in US 1.0 / JP */

    /* restorations */
    int betaCoin;            /* the beta gold coin in place of gold rings */
    int betaBombExplosion;
    int betaBoostGauge;      /* the beta boost/brake gauge, bomb and lives HUD */
    int szMissileBug;        /* Sector Z's missile cutscene, as JP 1.0 has it */

    /* checkpoints, by level */
    XboxCheckpoint cp[XBOX_CP_LEVELS];
} XboxSettings;

extern XboxSettings gXboxCfg;

#define XBOX_CFG(name) (gXboxCfg.name)

/* Read sf64x.ini from the save game, falling back on the defaults. */
void xbox_settings_load(void);
/* Write it back, if anything changed since the last load or save. */
void xbox_settings_save(void);
/* Defaults, and the name/range of each field, for the menu. */
void xbox_settings_defaults(XboxSettings* s);

/* The pads as the menu and the cheats read them (Platform/xbox/
 * kos_maple_xbox.c): a pad's buttons as XPAD_* bits (UP/DOWN/LEFT/RIGHT
 * also count the stick past half travel, the DPAD_* bits are the D-pad
 * alone), -1 if none is plugged in; and, while captured, the game reads
 * every pad at rest. Declared here rather than in dc/maple.h because game
 * files include this, and the KallistiOS shim headers bring in <stdbool.h>,
 * whose bool is not the game's. */
#define XPAD_A          0x0001
#define XPAD_B          0x0002
#define XPAD_X          0x0004
#define XPAD_Y          0x0008
#define XPAD_BLACK      0x0010
#define XPAD_WHITE      0x0020
#define XPAD_LT         0x0040
#define XPAD_RT         0x0080
#define XPAD_START      0x0100
#define XPAD_BACK       0x0200
#define XPAD_UP         0x0400
#define XPAD_DOWN       0x0800
#define XPAD_LEFT       0x1000
#define XPAD_RIGHT      0x2000
#define XPAD_DPAD_DOWN  0x4000  /* the D-pad alone, not the stick */
#define XPAD_DPAD_UP    0x8000
#define XPAD_DPAD_LEFT  0x10000
#define XPAD_DPAD_RIGHT 0x20000
int xbox_pad_buttons(int port);
void xbox_pad_capture(int on);

/* Options that stand in for a piece of the game (Platform/xbox/
 * xbox_enhance.c): true when done, and the game's own code is skipped.
 * They take the game's types, so only game files (which include this
 * through global.h) see them. */
/* Widescreen this frame (Platform/xbox/pvr_xbox.c): the option, on a 16:9
 * screen. A 16:9 view reaches 4/3 as far to each side as the game's 4:3
 * one: XBOX_WS_XK scales the game's horizontal visibility tests, and
 * XBOX_WS_EXTRA is the extra width each side in N64 screen pixels. */
int xbox_widescreen_active(void);
int xbox_widescreen_available(void);
#define XBOX_WS_XK (xbox_widescreen_active() ? (4.0f / 3.0f) : 1.0f)
#define XBOX_WS_EXTRA 53
/* The same, exactly: the 16:9 picture is 120 * 16/9 N64 pixels either side
 * of the middle, the game's 160. */
#define XBOX_HUD_SHIFT (120.0f * 16.0f / 9.0f - 160.0f)

#if defined(GLOBAL_H)
/* Matrix_ToMtx: `dest` (in the frame's matrix pool) is about to hold `mf`. */
void XboxInterp_Record(const Mtx* dest, const f32 mf[4][4]);

/* 2D rectangles after this are drawn xoff N64 pixels to the right (left
 * when negative) -- how 2D reaches the 16:9 picture's sides, which the
 * GBI's unsigned rectangle coordinates cannot (src/gfx/gfx_retro_dc.c, the
 * 'XO' escape). Reset to 0 at every frame's start. */
/* 60 fps: the starfield's points lie between these, and a blended draw
 * moves them with the field (src/gfx/gfx_retro_dc.c, 'SB'/'SE'). */
#define gXboxStarsBegin(pkt)                                                  \
    {                                                                         \
        Gfx* _g = (Gfx*) (pkt);                                               \
        _g->words.w0 = 0x424C4E44;                                            \
        _g->words.w1 = 0x53420000 | (gPlayerNum & 3);                         \
    }
#define gXboxStarsEnd(pkt)                                                    \
    {                                                                         \
        Gfx* _g = (Gfx*) (pkt);                                               \
        _g->words.w0 = 0x424C4E44;                                            \
        _g->words.w1 = 0x53450000;                                            \
    }

/* split screen on a 16:9 picture: the pane whose 2D follows (1 left, 2
   right, 0 none: whole-screen 2D), src/gfx/gfx_retro_dc.c 'PS' */
#define gXboxPaneSide(pkt, side)                                              \
    {                                                                         \
        Gfx* _g = (Gfx*) (pkt);                                               \
        _g->words.w0 = 0x424C4E44;                                            \
        _g->words.w1 = 0x50530000 | ((side) & 3);                             \
    }

/* ... for player n's pane (0 and 2 on the left, 1 and 3 on the right) */
#define gXboxPaneOf(pkt, n) gXboxPaneSide(pkt, (gCamCount == 4) ? (((n) & 1) ? 2 : 1) : 0)

#define gXboxRectOffset(pkt, xoff)                                            \
    {                                                                         \
        Gfx* _g = (Gfx*) (pkt);                                               \
        _g->words.w0 = 0x424C4E44;                                            \
        _g->words.w1 = 0x584F0000 | ((u16) (s16) ((xoff) * 4));               \
    }

bool XboxEnh_GoldRingDraw(Item* item);
bool XboxEnh_GoldRingUpdate(Item* item);
bool XboxEnh_BoostGaugeDraw(void);
bool XboxEnh_BombCounterDraw(void);
bool XboxEnh_LivesCounterDraw(void);
bool XboxEnh_EnemyRadioRed(void);
bool XboxEnh_RadarMarkArwingDraw(s32 colorIdx);
bool XboxEnh_RadarMarkWolfenDraw(void);

/* The HUD at the 16:9 picture's edges (Platform/xbox/xbox_enhance.c): a
 * group of HUD drawing against the left (side -1) or right (+1) side of the
 * screen goes between XboxHud_Begin and XboxHud_End; inside, its 2D
 * rectangles move out by themselves, and a piece placed in 3D at depth z
 * adds XboxHud_PerspX(z) to its x. XboxHud_EdgeUnits(z) is that distance
 * whichever the side, for pieces that pick their own. */
void XboxHud_Begin(s32 side);
void XboxHud_End(void);
f32 XboxHud_PerspX(f32 z);
f32 XboxHud_EdgeUnits(f32 z);
bool XboxHud_RadioWide(void);
#endif

/* The menu (Platform/xbox/xbox_menu.c). */
int XboxMenu_IsOpen(void);
void XboxMenu_Update(void);         /* once a frame, after the game's own drawing */
void XboxCheats_DisplayPost(void);  /* once a frame, as Starship's DisplayPostUpdate */

/* The frame-time readout (the frameTimes option): the main loop's top, the
 * game's frame built, and the frame's work done (before the vblank wait,
 * src/gfx/gfx_dc.c). */
void XboxPerf_FrameStart(void);
void XboxPerf_LogicDone(void);
void XboxPerf_DrawBegin(void);
void XboxPerf_SubmitBegin(void);    /* pvr_scene_finish: the draw's Direct3D part begins */
void XboxPerf_WorkDone(void);
unsigned int XboxPerf_LastBusyUs(void);
int XboxPerf_LastDraws(void);

/* 60 fps (Platform/xbox/xbox_interp.c). The vertical blanks the next draw
 * waits for (0: gVIsPerFrame, the game's own pacing; src/gfx/gfx_dc.c). */
extern int gXboxDrawVIs;
void XboxInterp_FrameBegin(void);
void XboxInterp_FrameEnd(void);
void XboxInterp_Push(const void* ptr, int id);
void XboxInterp_Pop(void);
void XboxInterp_SetStep(int k, int n);
int XboxInterp_Draws(unsigned int busyUs, int draws);
const float* XboxInterp_Lookup(const void* addr);
void XboxInterp_Stats(int* recorded, int* blended);
/* The starfield's field offset and roll this frame (fox_bg.c), and for a
 * blended draw, how far to move its points (N64 pixels; 0 when not blending
 * or when the field wrapped). */
void XboxInterp_Starfield(float xField, float yField, float rollCos, float rollSin);
void XboxInterp_StarOffset(int pane, float* dx, float* dy);

#endif /* XBOX_SETTINGS_H */
