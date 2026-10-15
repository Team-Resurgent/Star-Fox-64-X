/* xbox_menu.c -- the port's options menu, and the cheats that act once a
 * frame (xbox_settings.h).
 *
 * BACK on any pad opens it over whatever is on screen; that pad then drives
 * it: up/down (D-pad or stick) choose, left/right or A change, B or BACK
 * close. While it is open the game reads every pad at rest
 * (xbox_pad_capture), and in a level the game itself stands still
 * (fox_play.c skips Play_Update, as Starship's debug pause does) while the
 * picture keeps being drawn. Closing saves sf64x.ini if anything changed.
 *
 * It is drawn with the game's own small font and rectangle fill, into the
 * frame's display list after everything the game drew (sys_main.c), so it
 * goes through the same renderer as the rest of the picture. The font has
 * capitals, digits and ! : - . only, which is what the labels use. */
#include "global.h"

#include <stdio.h>

#include "xbox_settings.h"

extern int xbox_pvr_tr_sorted; /* pvr_xbox.c */
void arch_exit(void);          /* kos_sys_xbox.c: back to the dashboard */
void Audio_XboxUnmute(void);   /* src/audio/audio_general.c */

/* ITEM_VOLUME: one of the game's own volumes (its sound options, kept in its
   save); ITEM_ACTION: A does `act`, `state` says what is shown at the right. */
enum { ITEM_HEADER, ITEM_BOOL, ITEM_INT, ITEM_VOLUME, ITEM_ACTION };

typedef struct {
    const char* label;
    int* val;
    u8 kind;
    s16 min, max, step;
    s8 audio;                   /* ITEM_VOLUME: AUDIO_TYPE_* */
    void (*act)(void);
    const char* (*state)(void);
} MenuItem;

#define HEADER(l) { l, NULL, ITEM_HEADER, 0, 0, 0, 0, NULL, NULL }
#define BOOL(l, f) { l, &gXboxCfg.f, ITEM_BOOL, 0, 1, 1, 0, NULL, NULL }
#define INT(l, f, lo, hi, st) { l, &gXboxCfg.f, ITEM_INT, lo, hi, st, 0, NULL, NULL }
#define VOLUME(l, t) { l, NULL, ITEM_VOLUME, 0, 99, 3, t, NULL, NULL }
#define ACTION(l, fn, st) { l, NULL, ITEM_ACTION, 0, 0, 0, 0, fn, st }

static void act_title(void);
static void act_dashboard(void);
static void act_checkpoint(void);
static const char* state_checkpoint(void);

static const MenuItem sItems[] = {
    HEADER("DISPLAY"),
    BOOL("WIDESCREEN", widescreen),
    BOOL("HUD AT SCREEN EDGES", hudWide),
    BOOL("RADIO AT SCREEN EDGE", radioWide),
    BOOL("SORT TRANSLUCENCY", trSorted),
    BOOL("60 FPS", fps60),
    BOOL("SHOW FRAME TIME", frameTimes),
    HEADER("CONTROLS"),
    BOOL("INVERT Y AXIS", invertY),
    BOOL("CHARGE ON BLACK", lToCharge),
    HEADER("AUDIO"),
    INT("MASTER VOLUME", masterVolume, 0, 100, 5),
    VOLUME("MUSIC VOLUME", AUDIO_TYPE_MUSIC),
    VOLUME("VOICE VOLUME", AUDIO_TYPE_VOICE),
    VOLUME("SFX VOLUME", AUDIO_TYPE_SFX),
    HEADER("CHEATS"),
    BOOL("INFINITE LIVES", infiniteLives),
    BOOL("INVINCIBLE", invincible),
    BOOL("UNBREAKABLE WINGS", unbreakableWings),
    BOOL("INFINITE BOMBS", infiniteBombs),
    BOOL("INFINITE BOOST", infiniteBoost),
    BOOL("HYPER LASER", hyperLaser),
    BOOL("RAPID FIRE", rapidFire),
    INT("LASER RANGE", laserRange, 15, 800, 5),
    BOOL("D-PAD DOWN SELF DESTRUCT", selfDestruct),
    BOOL("START WITH FALCO DOWN", falcoDead),
    BOOL("START WITH SLIPPY DOWN", slippyDead),
    BOOL("START WITH PEPPY DOWN", peppyDead),
    BOOL("REPAIR WINGMEN", repairWingmen),
    BOOL("LEVEL SELECT", levelSelect),
    BOOL("LEVEL SELECT: NO BRIEFING", skipBriefing),
    BOOL("UNLOCK EXPERT MODE", forceExpert),
    BOOL("SFX JUKEBOX: WHITE", sfxJukebox),
    HEADER("ENHANCEMENTS"),
    BOOL("TEAM FACES IN COCKPITS", teamFaces),
    BOOL("RED ENEMY RADIO", enemyRedRadio),
    INT("COCKPIT GLASS", cockpitOpacity, 0, 255, 5),
    BOOL("NO GORGON FLASHES", disableGorgonFlash),
    BOOL("RADAR OUTLINES", fighterOutlines),
    BOOL("MACBETH CAMERA FIX", maCameraFix),
    BOOL("SECTOR Z: ALL FIGHTERS", szActorFix),
    BOOL("KATINA: NO LOW DETAIL", disableLOD),
    BOOL("HIT COUNT CAP 999", hitCap999),
    HEADER("RESTORATIONS"),
    BOOL("BETA GOLD COIN", betaCoin),
    BOOL("BETA BOMB EXPLOSION", betaBombExplosion),
    BOOL("BETA BOOST GAUGE", betaBoostGauge),
    BOOL("SECTOR Z MISSILE BUG", szMissileBug),
    HEADER("GAME"),
    ACTION("RETURN TO TITLE", act_title, NULL),
    ACTION("QUIT TO DASHBOARD", act_dashboard, NULL),
    ACTION("LEVEL CHECKPOINT", act_checkpoint, state_checkpoint),
};

#define NITEMS ((s32) ARRAY_COUNT(sItems))
#define ROWS 17         /* lines of items shown at once */

static s32 sOpen;
static s32 sPad;        /* the pad driving it */
static s32 sSel = 1;
static s32 sTop;
static s32 sHeld;       /* buttons held last frame, of the menu's pad */
static s32 sRepeat;     /* frames a direction has been held */
static s32 sReleasing;  /* closed, holding the game's input until the buttons are let go */
static s32 sPrevAny[4]; /* BACK held last frame, per pad (opening edge) */
static s32 sGameSaveDirty; /* a game volume changed: write the game's save on closing */

int XboxMenu_IsOpen(void) {
    return sOpen;
}

static void apply(void) {
    xbox_pvr_tr_sorted = gXboxCfg.trSorted;
}

static void step_sel(s32 dir) {
    s32 i = sSel;

    do {
        i += dir;
        if (i < 0) {
            i = NITEMS - 1;
        }
        if (i >= NITEMS) {
            i = 0;
        }
    } while (sItems[i].kind == ITEM_HEADER);
    sSel = i;
    if (sSel < sTop + 1) {
        sTop = sSel - 1;
    }
    if (sSel >= sTop + ROWS) {
        sTop = sSel - ROWS + 1;
    }
    if (sTop < 0) {
        sTop = 0;
    }
    if (sTop > NITEMS - ROWS) {
        sTop = NITEMS - ROWS;
    }
}

/* the game's own volume settings live in its save (fox_option.c's sound
   menu sets the same three) */
static u8* game_volume(s32 type) {
    switch (type) {
        case AUDIO_TYPE_MUSIC:
            return &gSaveFile.save.data.musicVolume;
        case AUDIO_TYPE_VOICE:
            return &gSaveFile.save.data.voiceVolume;
        default:
            return &gSaveFile.save.data.sfxVolume;
    }
}

static void change(s32 dir) {
    const MenuItem* it = &sItems[sSel];
    s32 v;

    if (it->kind == ITEM_ACTION) {
        return; /* A only (input) */
    }
    if (it->kind == ITEM_VOLUME) {
        u8* vol = game_volume(it->audio);

        v = *vol + dir * it->step;
        if (v < it->min) {
            v = it->min;
        }
        if (v > it->max) {
            v = it->max;
        }
        if (v != *vol) {
            /* as Option_Sound_SetVolumeLevels does */
            *vol = v;
            gVolumeSettings[it->audio] = v;
            Audio_SetVolume(it->audio, v);
            sGameSaveDirty = true;
            AUDIO_PLAY_SFX(NA_SE_CURSOR, gDefaultSfxSource, 4);
        }
        return;
    }
    v = *it->val;
    if (it->kind == ITEM_BOOL) {
        v = !v;
    } else {
        v += dir * it->step;
        if (v < it->min) {
            v = (dir < 0 && *it->val == it->min) ? it->max : it->min;
        }
        if (v > it->max) {
            v = (dir > 0 && *it->val == it->max) ? it->min : it->max;
        }
    }
    *it->val = v;
    apply();
    AUDIO_PLAY_SFX(NA_SE_CURSOR, gDefaultSfxSource, 4);
}

static void close_menu(void) {
    sOpen = false;
    sReleasing = true;
    xbox_settings_save();
    if (sGameSaveDirty) {
        sGameSaveDirty = false;
        Save_Write();
    }
    AUDIO_PLAY_SFX(NA_SE_CANCEL, gDefaultSfxSource, 4);
}

/* Starship's Reset (F4): the game from its boot -- the Nintendo logo, the
   save read, the title -- as gNextGameState takes it, through
   Game_SetGameState's clean-up of the level and the music. */
static void act_title(void) {
    if ((gGameState < GSTATE_TITLE) || (gGameState >= GSTATE_BOOT)) {
        return; /* booting: already on its way there */
    }
    close_menu();
    Audio_XboxUnmute();
    gNextGameState = GSTATE_BOOT;
}

static void act_dashboard(void) {
    close_menu();
    arch_exit();
}

/* A level's checkpoint (Starship's developer Set / Clear Checkpoint, read
   back at the level's start, fox_play.c): where the player is now, on a
   level that runs along its path. */
static bool checkpoint_level(void) {
    return (gGameState == GSTATE_PLAY) && (gCurrentLevel >= 0) && (gCurrentLevel < XBOX_CP_LEVELS);
}

static bool checkpoint_here_allowed(void) {
    return checkpoint_level() && (gLevelMode == LEVELMODE_ON_RAILS) && (gPlayer[0].state == PLAYERSTATE_ACTIVE);
}

/* where the player is now becomes the level's start */
static void checkpoint_set_here(void) {
    XboxCheckpoint* cp = &gXboxCfg.cp[gCurrentLevel];

    cp->set = true;
    cp->ground = gGroundSurface;
    cp->progress = (s32) ((-gPlayer[0].pos.z) - 250.0f);
    cp->index = gObjectLoadIndex;
}

static s32 sCheckpointMsg; /* frames left of "CHECKPOINT SET" */

/* the menu entry: A clears the level's checkpoint, or sets one here */
static void act_checkpoint(void) {
    if (!checkpoint_level()) {
        return;
    }
    if (gXboxCfg.cp[gCurrentLevel].set) {
        gXboxCfg.cp[gCurrentLevel].set = false;
    } else if (checkpoint_here_allowed()) {
        checkpoint_set_here();
    } else {
        return;
    }
    AUDIO_PLAY_SFX(NA_SE_DECIDE, gDefaultSfxSource, 4);
}

static const char* state_checkpoint(void) {
    if (!checkpoint_level()) {
        return "-";
    }
    if (gXboxCfg.cp[gCurrentLevel].set) {
        return "SET";
    }
    return ((gLevelMode == LEVELMODE_ON_RAILS) && (gPlayer[0].state == PLAYERSTATE_ACTIVE)) ? "OFF" : "-";
}

static void input(void) {
    s32 b, press, i;

    if (!sOpen) {
        if (sReleasing) {
            /* keep the game's input until the closing press is let go */
            b = xbox_pad_buttons(sPad);
            if (b <= 0) {
                sReleasing = false;
                xbox_pad_capture(false);
            }
        }
        for (i = 0; i < 4; i++) {
            b = xbox_pad_buttons(i);
            if (b < 0) {
                sPrevAny[i] = 0;
                continue;
            }
            if ((b & XPAD_BACK) && !sPrevAny[i] && !sReleasing) {
                sOpen = true;
                sPad = i;
                sHeld = b;
                sRepeat = 0;
                xbox_pad_capture(true);
                AUDIO_PLAY_SFX(NA_SE_DECIDE, gDefaultSfxSource, 4);
            }
            sPrevAny[i] = (b & XPAD_BACK) != 0;
        }
        return;
    }

    b = xbox_pad_buttons(sPad);
    if (b < 0) { /* the pad was pulled out */
        close_menu();
        return;
    }
    press = b & ~sHeld;
    sHeld = b;
    sPrevAny[sPad] = (b & XPAD_BACK) != 0;

    if (press & (XPAD_B | XPAD_BACK)) {
        close_menu();
        return;
    }
    if (b & (XPAD_UP | XPAD_DOWN | XPAD_LEFT | XPAD_RIGHT)) {
        sRepeat++;
    } else {
        sRepeat = 0;
    }
    /* a held direction repeats after a quarter second, every other frame */
    if ((sRepeat > 8) && (sRepeat % 2 == 0)) {
        press |= b & (XPAD_UP | XPAD_DOWN | XPAD_LEFT | XPAD_RIGHT);
    }
    if (press & XPAD_UP) {
        step_sel(-1);
        AUDIO_PLAY_SFX(NA_SE_CURSOR, gDefaultSfxSource, 4);
    } else if (press & XPAD_DOWN) {
        step_sel(1);
        AUDIO_PLAY_SFX(NA_SE_CURSOR, gDefaultSfxSource, 4);
    } else if (press & XPAD_LEFT) {
        change(-1);
    } else if ((press & XPAD_A) && (sItems[sSel].kind == ITEM_ACTION)) {
        sItems[sSel].act();
    } else if (press & (XPAD_RIGHT | XPAD_A)) {
        change(1);
    }
}

static void draw(void) {
    char num[12];
    s32 row, i, y;
    s32 x0 = 24, x1 = 296, y0 = 14, y1 = 226;

    Graphics_FillRectangle(&gMasterDisp, x0, y0, x1, y1, 0, 0, 24, 208);
    Graphics_FillRectangle(&gMasterDisp, x0, y0, x1, y0 + 1, 120, 120, 255, 255);
    Graphics_FillRectangle(&gMasterDisp, x0, y1 - 1, x1, y1, 120, 120, 255, 255);

    RCP_SetupDL(&gMasterDisp, SETUPDL_83);
    gDPSetTextureFilter(gMasterDisp++, G_TF_POINT);

    gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 255, 0, 255);
    Graphics_DisplaySmallText(x0 + 8, y0 + 5, 1.0f, 1.0f, "STAR FOX 64 X OPTIONS");

    for (row = 0; row < ROWS; row++) {
        const MenuItem* it;

        i = sTop + row;
        if (i >= NITEMS) {
            break;
        }
        it = &sItems[i];
        y = y0 + 20 + row * 10;
        if (it->kind == ITEM_HEADER) {
            gDPSetPrimColor(gMasterDisp++, 0, 0, 120, 160, 255, 255);
            Graphics_DisplaySmallText(x0 + 8, y, 1.0f, 1.0f, (char*) it->label);
            continue;
        }
        if (i == sSel) {
            gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 255, 0, 255);
        } else {
            gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 255, 255, 255);
        }
        Graphics_DisplaySmallText(x0 + 16, y, 1.0f, 1.0f, (char*) it->label);
        if ((it->val == &gXboxCfg.widescreen) && !xbox_widescreen_available()) {
            /* a 4:3 screen: set 720p, or widescreen at 480, in the dashboard */
            Graphics_DisplaySmallText(x1 - 8 - Graphics_GetSmallTextWidth("4:3 TV"), y, 1.0f, 1.0f, "4:3 TV");
        } else if (it->kind == ITEM_ACTION) {
            if (it->state != NULL) {
                const char* s = it->state();

                Graphics_DisplaySmallText(x1 - 8 - Graphics_GetSmallTextWidth((char*) s), y, 1.0f, 1.0f, (char*) s);
            }
        } else if (it->kind == ITEM_VOLUME) {
            snprintf(num, sizeof(num), "%d", *game_volume(it->audio));
            Graphics_DisplaySmallText(x1 - 8 - Graphics_GetSmallTextWidth(num), y, 1.0f, 1.0f, num);
        } else if (it->kind == ITEM_BOOL) {
            Graphics_DisplaySmallText(x1 - 32, y, 1.0f, 1.0f, *it->val ? "ON" : "OFF");
        } else {
            snprintf(num, sizeof(num), "%d", *it->val);
            Graphics_DisplaySmallText(x1 - 8 - Graphics_GetSmallTextWidth(num), y, 1.0f, 1.0f, num);
        }
    }

    gDPSetPrimColor(gMasterDisp++, 0, 0, 160, 160, 160, 255);
    Graphics_DisplaySmallText(x0 + 8, y1 - 11, 1.0f, 1.0f, "A: CHANGE   B: CLOSE");
}

/* ------------------------------------------------- frame-time readout
 * Per logic frame: the game's logic (input to display list built), each
 * draw of it (to the moment the renderer waits for the vertical blank:
 * one draw at 30 fps, two or more with the 60 fps option), and the two
 * together -- averaged over half a second, with the worst frame in it. A
 * logic frame has gVIsPerFrame blanks of 16.7 ms; at 60 fps its logic and
 * all its draws must fit in them. On screen, not printed: the debug
 * channel stalls the game once its buffer fills. The 60 fps fallback reads
 * the last frame's figures (XboxPerf_LastBusyUs). */
uint64_t timer_us_gettime64(void); /* arch/timer.h (Platform/xbox/kos_sys_xbox.c) */

static uint64_t sPerfT0, sDrawT0, sSubT0;
static uint32_t sPerfLogic, sPerfDraw, sPerfDraws, sPerfSub;  /* this logic frame, us */
static uint32_t sLastBusy, sLastDraws;
static uint32_t sSumLogic, sSumDraw, sSumDraws, sSumBusy, sMaxBusy, sSumSub, sPerfN;
static uint32_t sShowLogic, sShowDraw, sShowBusy, sShowMax, sShowDraws, sShowSub; /* the last half second's */

void XboxPerf_FrameStart(void) {
    uint64_t now = timer_us_gettime64();

    if (sPerfDraws != 0) {
        uint32_t busy = sPerfLogic + sPerfDraw;

        sLastBusy = busy;
        sLastDraws = sPerfDraws;
        sSumLogic += sPerfLogic;
        sSumDraw += sPerfDraw;
        sSumDraws += sPerfDraws;
        sSumSub += sPerfSub;
        sSumBusy += busy;
        if (busy > sMaxBusy) {
            sMaxBusy = busy;
        }
        if (++sPerfN >= 15) { /* half a second of 30 fps frames */
            sShowLogic = sSumLogic / sPerfN;
            sShowDraw = sSumDraw / sSumDraws;
            sShowBusy = sSumBusy / sPerfN;
            sShowMax = sMaxBusy;
            sShowDraws = (sSumDraws + sPerfN / 2) / sPerfN;
            sShowSub = sSumSub / sSumDraws;
            sSumLogic = sSumDraw = sSumDraws = sSumBusy = sMaxBusy = sSumSub = sPerfN = 0;
        }
    }
    sPerfT0 = now;
    sPerfDraw = 0;
    sPerfDraws = 0;
    sPerfSub = 0;
}

void XboxPerf_LogicDone(void) {
    sPerfLogic = (uint32_t) (timer_us_gettime64() - sPerfT0);
}

void XboxPerf_DrawBegin(void) {
    sDrawT0 = timer_us_gettime64();
    sSubT0 = 0;
}

/* Of a draw, the part from pvr_scene_finish on: the Direct3D calls, the
   Present (which waits when the GPU is two frames behind) and the texture
   housekeeping. The rest of DRAW is the display list's interpretation. */
void XboxPerf_SubmitBegin(void) {
    sSubT0 = timer_us_gettime64();
}

void XboxPerf_WorkDone(void) {
    uint64_t now = timer_us_gettime64();

    sPerfDraw += (uint32_t) (now - sDrawT0);
    if (sSubT0 != 0) {
        sPerfSub += (uint32_t) (now - sSubT0);
    }
    sPerfDraws++;
}

unsigned int XboxPerf_LastBusyUs(void) {
    return sLastBusy;
}

int XboxPerf_LastDraws(void) {
    return (int) sLastDraws;
}

static void ms_text(char* out, s32 n, const char* label, uint32_t us) {
    snprintf(out, n, "%s %u.%u", label, (unsigned) (us / 1000), (unsigned) ((us % 1000) / 100));
}

static void draw_perf(void) {
    char t[24];

    /* the bottom left corner, as the original readout */
    Graphics_FillRectangle(&gMasterDisp, 16, 164, 128, 228, 0, 0, 0, 160);
    RCP_SetupDL(&gMasterDisp, SETUPDL_83);
    gDPSetTextureFilter(gMasterDisp++, G_TF_POINT);
    gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 255, 255, 255);
    if (XBOX_CFG(fps60)) {
        int rec, bl;

        /* 60 fps: the matrices of the frame, and how many the extra draw blended */
        XboxInterp_Stats(&rec, &bl);
        snprintf(t, sizeof(t), "BLEND %d-%d", bl, rec);
        Graphics_DisplaySmallText(20, 166, 1.0f, 1.0f, t);
    }
    ms_text(t, sizeof(t), "LOGIC", sShowLogic);
    Graphics_DisplaySmallText(20, 176, 1.0f, 1.0f, t);
    ms_text(t, sizeof(t), "DRAW", sShowDraw);
    snprintf(t + strlen(t), sizeof(t) - strlen(t), " X%u", (unsigned) sShowDraws);
    Graphics_DisplaySmallText(20, 186, 1.0f, 1.0f, t);
    ms_text(t, sizeof(t), " D3D", sShowSub);
    Graphics_DisplaySmallText(20, 196, 1.0f, 1.0f, t);
    ms_text(t, sizeof(t), "FRAME", sShowBusy);
    Graphics_DisplaySmallText(20, 206, 1.0f, 1.0f, t);
    ms_text(t, sizeof(t), "WORST", sShowMax);
    Graphics_DisplaySmallText(20, 216, 1.0f, 1.0f, t);
}

/* (the settings are loaded, and the renderer's set, by main before the
   game starts: Platform/xbox/xbox_main.c) */
void XboxMenu_Update(void) {
    input();
    if (XBOX_CFG(frameTimes)) {
        draw_perf();
    }
    if (sCheckpointMsg > 0) {
        sCheckpointMsg--;
        if (!sOpen && (gGameState == GSTATE_PLAY)) {
            RCP_SetupDL(&gMasterDisp, SETUPDL_83);
            gDPSetTextureFilter(gMasterDisp++, G_TF_POINT);
            gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 255, 0, 255);
            Graphics_DisplaySmallText(160 - Graphics_GetSmallTextWidth("CHECKPOINT SET") / 2, 70, 1.0f, 1.0f,
                                      "CHECKPOINT SET");
        }
    }
    if (sOpen) {
        draw();
    }
}

/* Starship's per-frame cheats (src/port/mods/PortEnhancements.c
 * OnDisplayUpdatePost and Hit64_Main): in a level, after its frame. */
void XboxCheats_DisplayPost(void) {
    static s32 sDpadDownHeld;
    static s32 sWhiteHeld;
    s32 b;

    if ((gGameState != GSTATE_PLAY) || (gPlayState <= PLAY_INIT)) {
        return;
    }
    if (XBOX_CFG(infiniteLives)) {
        gLifeCount[0] = 9;
    }
    if (XBOX_CFG(infiniteBombs)) {
        gBombCount[0] = 9;
    }
    if (XBOX_CFG(hyperLaser)) {
        gLaserStrength[0] = LASERS_HYPER;
    }
    /* Hit64's self destruct, on the D-pad's down, which the game's pad
       mapping leaves free (the N64 one was on its D-pad down too) */
    b = sOpen ? 0 : xbox_pad_buttons(gMainController);
    if (b < 0) {
        b = 0;
    }
    /* only a ship that is flying: not paused, not in a level's intro or
       outro, not already going down (a second blast into the death sequence,
       or one while paused, is not something the game ever does) */
    if (XBOX_CFG(selfDestruct) && (b & XPAD_DPAD_DOWN) && !sDpadDownHeld && (gPlayState == PLAY_UPDATE) &&
        (gPlayer[0].state == PLAYERSTATE_ACTIVE) && (gPlayer[0].shields > 0)) {
        Player* player = &gPlayer[0];

        player->shields = 0;
        Player_ApplyDamage(player, 0, 10000);
    }
    sDpadDownHeld = (b & XPAD_DPAD_DOWN) != 0;

    /* WHITE, which the game does not use in a level: the level's
       checkpoint, here (the GAME menu's LEVEL CHECKPOINT shows and clears it) */
    if ((b & XPAD_WHITE) && !sWhiteHeld && (gPlayState != PLAY_PAUSE) && checkpoint_here_allowed()) {
        checkpoint_set_here();
        xbox_settings_save();
        sCheckpointMsg = 45;
        AUDIO_PLAY_SFX(NA_SE_DECIDE, gDefaultSfxSource, 4);
    }
    sWhiteHeld = (b & XPAD_WHITE) != 0;
}
