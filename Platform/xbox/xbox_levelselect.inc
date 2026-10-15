/* xbox_levelselect.inc -- the map's level select, a cheat in the port's
 * menu (xbox_settings.h levelSelect / skipBriefing).
 *
 * Included into src/overlays/ovl_menu/fox_map.c in place of the decomp's
 * compile-time src/mods/levelselect.c, for the map's own state. It is that
 * mod as Starship runs it (src/mods/levelselect.c there: only while the
 * map is idle or zooming in, the choice drawn at the foot of the screen,
 * the briefing skipped on request and always for Venom's choices), read
 * from the Xbox pad: the N64 mod is driven by the N64 D-pad, which the
 * game's Xbox mapping does not produce (src/sys/sys_joybus.c gives the
 * Xbox D-pad's up and right to the C buttons), so the D-pad is read here
 * directly:
 *
 *   D-pad left / right   mission 1..7
 *   D-pad up / down      route (easy / normal / hard planet of the mission)
 *   WHITE                the planet's other start: warp zone, Andross,
 *                        Area 6's beta stage
 *   A                    go
 */
extern PlanetId sCurrentPlanetId;
extern s32 D_menu_801CD968;
extern s32 sMapSubState;

void Map_LevelStart_AudioSpecSetup(LevelId level);
void Map_CurrentLevel_Setup(void);
void Map_PositionCursor(void);
void Map_PlayLevel(void);

static PlanetId sXboxLsPlanetArray[][3] = {
    { PLANET_CORNERIA, PLANET_CORNERIA, PLANET_CORNERIA }, { PLANET_METEO, PLANET_METEO, PLANET_SECTOR_Y },
    { PLANET_FORTUNA, PLANET_KATINA, PLANET_AQUAS },       { PLANET_SECTOR_X, PLANET_SOLAR, PLANET_ZONESS },
    { PLANET_TITANIA, PLANET_MACBETH, PLANET_SECTOR_Z },   { PLANET_BOLSE, PLANET_BOLSE, PLANET_AREA_6 },
    { PLANET_VENOM, PLANET_VENOM, SAVE_SLOT_VENOM_2 },
};

void Map_LevelSelect(void) {
    static s32 mission = 0;
    static s32 difficulty = 0;
    static char* sLevelSelectPlanetNames[] = {
        "METEO",  "AREA 6",   "BOLSE",   "SECTOR Z", "SECTOR X", "SECTOR Y", "KATINA", "MACBETH",
        "ZONESS", "CORNERIA", "TITANIA", "AQUAS",    "FORTUNA",  "VENOM 1",  "SOLAR",  "VENOM 2",
    };
    static s32 startOption = 0;
    static s32 timer = 30;
    static s32 startLevel = 0;
    static s32 held = 0;
    s32 pad, press;
    s32 nextPlanetId;

    if ((sMapState != MAP_IDLE) && (sMapState != MAP_ZOOM_PLANET)) {
        return;
    }

    pad = XboxMenu_IsOpen() ? 0 : xbox_pad_buttons(gMainController);
    if (pad < 0) {
        pad = 0;
    }
    press = pad & ~held;
    held = pad;

    if (press & XPAD_DPAD_LEFT) {
        mission--;
        if (mission < 0) {
            mission = 6;
        }
    } else if (press & XPAD_DPAD_RIGHT) {
        mission++;
        if (mission > 6) {
            mission = 0;
        }
    } else if ((press & XPAD_DPAD_UP) && (mission != 0)) {
        difficulty++;
        if (difficulty > 2) {
            difficulty = 0;
        }
        if ((difficulty == 1) && ((mission == 1) || (mission == 5) || (mission == 6))) {
            difficulty = 2;
        }
    } else if ((press & XPAD_DPAD_DOWN) && (mission != 0)) {
        difficulty--;
        if ((difficulty != 2) && ((mission == 1) || (mission == 5) || (mission == 6))) {
            difficulty--;
        }
        if (difficulty < 0) {
            difficulty = 2;
        }
    }

    nextPlanetId = (sXboxLsPlanetArray[mission][difficulty] == SAVE_SLOT_VENOM_2)
                       ? PLANET_VENOM
                       : sXboxLsPlanetArray[mission][difficulty];
    if (sCurrentPlanetId != nextPlanetId) {
        sCurrentPlanetId = nextPlanetId;
        startOption = 0;
        Map_CurrentLevel_Setup();
        Map_PositionCursor();
    }
    if (press & XPAD_WHITE) {
        startOption ^= 1;
    }

    /* Draw */
    if ((sCurrentPlanetId >= 0) && (sCurrentPlanetId < PLANET_MAX)) {
        s32 y = 225;

        RCP_SetupDL(&gMasterDisp, SETUPDL_83);
        gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 255, 0, 255);

        Graphics_DisplaySmallText(20, y, 1.0f, 1.0f, "PLANET:");
        Graphics_DisplaySmallText(80, y, 1.0f, 1.0f,
                                  sLevelSelectPlanetNames[sXboxLsPlanetArray[mission][difficulty]]);

        if (startOption) {
            if ((sCurrentPlanetId == PLANET_SECTOR_X) || (sCurrentPlanetId == PLANET_METEO)) {
                Graphics_DisplaySmallText(80 + 60 + 10, y, 1.0f, 1.0f, "WARP ZONE");
            } else if (sCurrentPlanetId == PLANET_VENOM) {
                Graphics_DisplaySmallText(80 + 60 + 3, y, 1.0f, 1.0f, "ANDROSS");
            } else if (sCurrentPlanetId == PLANET_AREA_6) {
                Graphics_DisplaySmallText(80 + 60 - 3, y, 1.0f, 1.0f, "BETA SB");
            }
        }
    }

    if (gControllerPress[0].button & A_BUTTON) {
        timer = 15;
        startLevel = 1;
    }

    if (timer > 0) {
        timer--;
    }

    // Bypass briefing
    if (XBOX_CFG(skipBriefing) || (sCurrentPlanetId == PLANET_VENOM)) {
        if ((timer == 0) && (startLevel == 1)) {
            if ((sMapState == 2) && (sMapSubState > 0)) {
                if (sCurrentPlanetId == PLANET_VENOM) {
                    if (startOption) {
                        gCurrentLevel = LEVEL_VENOM_ANDROSS;
                    } else if (sXboxLsPlanetArray[mission][difficulty] == SAVE_SLOT_VENOM_2) {
                        gCurrentLevel = LEVEL_VENOM_2;
                    }
                } else if ((sCurrentPlanetId == PLANET_AREA_6) && startOption) {
                    gCurrentLevel = LEVEL_UNK_4;
                }
                Map_LevelStart_AudioSpecSetup(gCurrentLevel);
                sLevelStartState = 0;
                D_menu_801CD968 = 0;
                Map_PlayLevel();
                if (startOption && ((gCurrentLevel == LEVEL_METEO) || (gCurrentLevel == LEVEL_SECTOR_X) ||
                                    (sXboxLsPlanetArray[mission][difficulty] == SAVE_SLOT_VENOM_2))) {
                    gLevelPhase = 1;
                }
            }
        }
    } else {
        if (startOption && ((gCurrentLevel == LEVEL_METEO) || (gCurrentLevel == LEVEL_SECTOR_X) ||
                            (sXboxLsPlanetArray[mission][difficulty] == SAVE_SLOT_VENOM_2))) {
            gLevelPhase = 1;
        }
    }
}
