/* xbox_enhance.c -- the options that replace a piece of the game's drawing
 * or behaviour outright (xbox_settings.h): Starship's event listeners
 * (HarbourMasters/Starship src/port/mods/PortEnhancements.c), called from
 * the game at the points where Starship raises those events. Each returns
 * true when it has done the job, and the game's own code is skipped.
 */
#include "global.h"
#include "assets/ast_star_wolf.h"

#include "xbox_beta_gauge.h"

/* ---------------------------------------------- restoration: beta coin
 * The gold ring as the beta's coin: its model, and three for a shield
 * upgrade, six for a life (Starship's OnItemGoldRingDraw / Update). */
bool XboxEnh_GoldRingDraw(Item* item) {
    if ((item->obj.id != OBJ_ITEM_GOLD_RING) || !XBOX_CFG(betaCoin)) {
        return false;
    }
    RCP_SetupDL(&gMasterDisp, SETUPDL_29);
    Graphics_SetScaleMtx(item->width * 2.0f);
    gSPDisplayList(gMasterDisp++, D_101D870);
    return true;
}

bool XboxEnh_GoldRingUpdate(Item* item) {
    if ((item->obj.id != OBJ_ITEM_GOLD_RING) || !XBOX_CFG(betaCoin)) {
        return false;
    }

    Item_CheckBounds(item);
    switch (item->state) {
        case 0:
            if (item->collected) {
                item->state = 1;
                item->timer_48 = 0;
                gGoldRingCount[0]++;
                if (gGoldRingCount[0] == 3) {
                    Object_PlayerSfx(gPlayer[item->playerNum].sfxSource, NA_SE_SHIELD_UPGRADE, item->playerNum);
                } else if (gGoldRingCount[0] == 6) {
                    Object_PlayerSfx(gPlayer[item->playerNum].sfxSource, NA_SE_ONE_UP, item->playerNum);
                    if (gCurrentLevel != LEVEL_TRAINING) {
                        gLifeCount[item->playerNum]++;
                    }
                    gPlayer[item->playerNum].heal += 32;
                    BonusText_Display(gPlayer[item->playerNum].pos.x, gPlayer[item->playerNum].pos.y,
                                      gPlayer[item->playerNum].trueZpos, BONUS_TEXT_1UP);
                } else {
                    gPlayer[item->playerNum].heal += 32;
                    Object_PlayerSfx(gPlayer[item->playerNum].sfxSource, NA_SE_GOLD_RING, item->playerNum);
                }
            }

            if (item->timer_48 == 1) {
                Object_Kill(&item->obj, item->sfxSource);
            }
            break;

        case 1:
            ItemSupplyRing_Update(item);
            break;
    }
    return true;
}

/* ---------------------------------------- restoration: beta boost gauge
 * The beta's dial (from the ROM) with Starship's needle, and the bomb and
 * life counters where the beta HUD puts them (OnBoostGaugeDraw,
 * OnBombCounterDraw, OnLivesCounterDraw). */
bool XboxEnh_BoostGaugeDraw(void) {
    u8 step;
    f32 x = 70.0f;
    f32 y = 30.0f;
    s32 n;

    if (!XBOX_CFG(betaBoostGauge)) {
        return false;
    }
    n = (s32) ((gPlayer[0].boostMeter / 90.0f) * ARRAY_COUNT(sXboxBoostGaugeArrow));
    step = (n < 8) ? (u8) n : 8;

    RCP_SetupDL(&gMasterDisp, SETUPDL_76);
    gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 255, 255, 255);
    Lib_TextureRect_CI8(&gMasterDisp, D_1012290, D_10126B0, 48, 22, SCREEN_WIDTH - x, y, 1.0f, 1.0f);
    Lib_TextureRect_CI8(&gMasterDisp, D_10126F0, D_1012750, 24, 4, SCREEN_WIDTH - (x - 9), y + 3, 1.0f, 1.0f);
    Lib_TextureRect_RGBA16(&gMasterDisp, sXboxBoostGaugeArrow[step], 32, 32, SCREEN_WIDTH - (x - 6), y - 1, 0.9f,
                           0.9f);
    return true;
}

bool XboxEnh_BombCounterDraw(void) {
    if (!XBOX_CFG(betaBoostGauge)) {
        return false;
    }
    HUD_BombCounter_Draw(253.0f, 18.0f);
    return true;
}

bool XboxEnh_LivesCounterDraw(void) {
    if (!XBOX_CFG(betaBoostGauge)) {
        return false;
    }
    /* Starship puts it at the bottom right (y 220), which runs into the
       8-pixel border the game keeps clear round the screen (the overscan
       mask, src/gfx/gfx_pvr.c) and, in all-range mode, into the radar: here it
       goes under the dial, with the bombs above it, all in one column */
    HUD_LivesCount2_Draw(258.0f, 60.0f, gLifeCount[gPlayerNum]);
    return true;
}

/* ------------------------------------------- enhancement: red enemy radio
 * Enemy voices in a red message box (OnPreSetupRadioMsgEvent). */
bool XboxEnh_EnemyRadioRed(void) {
    s32 id = (s32) gRadioMsgRadioId;

    if (!XBOX_CFG(enemyRedRadio)) {
        return false;
    }
    switch (id) {
        case RCID_BOSS_CORNERIA:
        case RCID_BOSS_CORNERIA2:
        case RCID_BOSS_METEO:
        case RCID_BOSS_SECTORX:
        case RCID_BOSS_SECTORY:
        case RCID_BOSS_MACBETH:
        case RCID_BOSS_ZONESS:
        case RCID_BOSS_AREA6:
        case RCID_CAIMAN_AREA6:
        case RCID_WOLF:
        case RCID_PIGMA:
        case RCID_LEON:
        case RCID_ANDREW:
        case RCID_WOLF_2:
        case RCID_PIGMA_2:
        case RCID_LEON_2:
        case RCID_ANDREW_2:
            return true;
    }
    return false;
}

/* -------------------------------------------- enhancement: radar outlines
 * Arwing and Wolfen marks drawn twice, a dark outline under the colour
 * (OnRadarMarkArwingDraw / OnRadarMarkWolfenDraw). */
bool XboxEnh_RadarMarkArwingDraw(s32 colorIdx) {
    static const s32 arwingMarkColor[][4] = {
        { 177, 242, 12, 255 }, { 89, 121, 6, 128 }, { 90, 90, 255, 255 }, { 45, 45, 128, 128 },
        { 0, 179, 67, 255 },   { 0, 90, 34, 128 },  { 255, 30, 0, 255 },  { 128, 15, 0, 128 },
    };
    f32 var_fv1;
    f32 var_fv2;

    if (!XBOX_CFG(fighterOutlines)) {
        return false;
    }

    if (gCamCount != 1) {
        var_fv1 = 38.0f;
        var_fv2 = 38.0f;
    } else {
        var_fv1 = 54.0f;
        var_fv2 = 54.0f;
    }

    RCP_SetupDL(&gMasterDisp, SETUPDL_62);
    gDPSetPrimColor(gMasterDisp++, 0, 0, 0, 0, 0, 255);
    Matrix_Scale(gGfxMatrix, var_fv1, var_fv2, 1.0f, MTXF_APPLY);
    Matrix_SetGfxMtx(&gMasterDisp);
    gSPDisplayList(gMasterDisp++, aRadarMarkArwingDL);

    gDPSetPrimColor(gMasterDisp++, 0, 0, arwingMarkColor[colorIdx][0], arwingMarkColor[colorIdx][1],
                    arwingMarkColor[colorIdx][2], arwingMarkColor[colorIdx][3]);
    Matrix_Translate(gGfxMatrix, 0.0f, 1.0f, 0.0f, MTXF_APPLY);
    Matrix_Scale(gGfxMatrix, 0.8f, 0.7f, 1.0f, MTXF_APPLY);
    Matrix_SetGfxMtx(&gMasterDisp);
    gSPDisplayList(gMasterDisp++, aRadarMarkArwingDL);
    return true;
}

bool XboxEnh_RadarMarkWolfenDraw(void) {
    if (!XBOX_CFG(fighterOutlines)) {
        return false;
    }

    RCP_SetupDL(&gMasterDisp, SETUPDL_62);
    gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 255, 255, 255);
    Matrix_Scale(gGfxMatrix, 54.0f, 54.0f, 1.0f, MTXF_APPLY);
    Matrix_SetGfxMtx(&gMasterDisp);
    gSPDisplayList(gMasterDisp++, aStarWolfRadarMarkDL);

    gDPSetPrimColor(gMasterDisp++, 0, 0, 0, 0, 0, 255);
    Matrix_Translate(gGfxMatrix, 0.0f, -1.2f, 0.0f, MTXF_APPLY);
    Matrix_Scale(gGfxMatrix, 0.9f, 0.8f, 1.0f, MTXF_APPLY);
    Matrix_SetGfxMtx(&gMasterDisp);
    gSPDisplayList(gMasterDisp++, aStarWolfRadarMarkDL);
    return true;
}

/* ------------------------------------------ display: HUD at the screen edges
 * Starship's HUD aspect ratio ("Expand"), on the 16:9 picture: what the game
 * puts against a side of its 4:3 screen goes the same distance from that side
 * of the 16:9 one -- XBOX_HUD_SHIFT N64 pixels further out. The game's HUD
 * code brackets each such group with XboxHud_Begin(side) .. XboxHud_End():
 * its 2D rectangles move through the renderer's rectangle offset (the 'XO'
 * escape, gXboxRectOffset), and what it places in 3D in front of the camera
 * asks XboxHud_PerspX(z) for the same move at that depth. Starship's own
 * per-level radar constants (gHudOffsetPers) are this same sum: 2.15 for
 * Corneria's radar at z -626 is 626 * tan(22.5) / 120. */
static s32 sXboxHudSide;

static bool hud_wide(void) {
    return XBOX_CFG(hudWide) && xbox_widescreen_active();
}

void XboxHud_Begin(s32 side) {
    sXboxHudSide = hud_wide() ? side : 0;
    if (sXboxHudSide != 0) {
        gXboxRectOffset(gMasterDisp++, sXboxHudSide * XBOX_HUD_SHIFT);
    }
}

void XboxHud_End(void) {
    if (sXboxHudSide != 0) {
        gXboxRectOffset(gMasterDisp++, 0);
        sXboxHudSide = 0;
    }
}

/* XBOX_HUD_SHIFT screen pixels as a distance at depth z, in the game's
   perspective (Lib_InitPerspective: gFovY, 4:3, 120 pixels from the middle
   to the top). */
f32 XboxHud_EdgeUnits(f32 z) {
    if (!hud_wide()) {
        return 0.0f;
    }
    return XBOX_HUD_SHIFT * fabsf(z) * tanf(gFovY * (F_PI / 360.0f)) / (SCREEN_HEIGHT / 2.0f);
}

f32 XboxHud_PerspX(f32 z) {
    return (sXboxHudSide != 0) ? sXboxHudSide * XboxHud_EdgeUnits(z) : 0.0f;
}

/* The radio box goes to the left edge with the HUD on the title screen (as
   Starship's title always puts it), and in a level with its own option
   (Starship's "Radio Communication Box: Expand"); the ending's stays. */
bool XboxHud_RadioWide(void) {
    return hud_wide() &&
           ((gGameState == GSTATE_TITLE) || ((gGameState == GSTATE_PLAY) && XBOX_CFG(radioWide)));
}
