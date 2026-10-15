/* xbox_settings.c -- the port's options (xbox_settings.h), kept as
 * key=value lines in sf64x.ini inside the title's save game on U:, next to
 * the memory card's files (kos_vmu_xbox.c leaves it out of the card).
 *
 * Unknown keys are ignored and missing ones keep their defaults, so the
 * file survives options being added or removed between builds. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kos/dbglog.h>

#include "xbox_settings.h"

XboxSettings gXboxCfg;
static XboxSettings sSaved;     /* what the file holds */

typedef struct {
    const char* key;
    size_t off;
    int def, min, max;
} Field;

#define F(name, def, min, max) { #name, offsetof(XboxSettings, name), def, min, max }
/* a level's checkpoint: cp<level>set, ...ground, ...progress, ...index */
#define CP(n)                                                                 \
    { "cp" #n "set", offsetof(XboxSettings, cp[n].set), 0, 0, 1 },            \
    { "cp" #n "ground", offsetof(XboxSettings, cp[n].ground), 0, 0, 255 },    \
    { "cp" #n "progress", offsetof(XboxSettings, cp[n].progress), 0, -1000000, 10000000 }, \
    { "cp" #n "index", offsetof(XboxSettings, cp[n].index), 0, 0, 100000 }

static const Field sFields[] = {
    F(widescreen, 0, 0, 1),
    F(trSorted, 0, 0, 1),
    F(fps60, 0, 0, 1),
    F(frameTimes, 0, 0, 1),
    F(hudWide, 1, 0, 1),
    F(radioWide, 0, 0, 1),
    F(invertY, 0, 0, 1),
    F(lToCharge, 0, 0, 1),
    F(masterVolume, 100, 0, 100),
    F(infiniteLives, 0, 0, 1),
    F(invincible, 0, 0, 1),
    F(unbreakableWings, 0, 0, 1),
    F(infiniteBombs, 0, 0, 1),
    F(infiniteBoost, 0, 0, 1),
    F(hyperLaser, 0, 0, 1),
    F(rapidFire, 0, 0, 1),
    F(laserRange, 100, 15, 800),
    F(selfDestruct, 0, 0, 1),
    F(falcoDead, 0, 0, 1),
    F(slippyDead, 0, 0, 1),
    F(peppyDead, 0, 0, 1),
    F(repairWingmen, 0, 0, 1),
    F(levelSelect, 0, 0, 1),
    F(skipBriefing, 0, 0, 1),
    F(forceExpert, 0, 0, 1),
    F(sfxJukebox, 0, 0, 1),
    F(teamFaces, 1, 0, 1),
    F(enemyRedRadio, 0, 0, 1),
    F(cockpitOpacity, 120, 0, 255),
    F(disableGorgonFlash, 0, 0, 1),
    F(fighterOutlines, 0, 0, 1),
    F(maCameraFix, 0, 0, 1),
    F(szActorFix, 0, 0, 1),
    F(disableLOD, 0, 0, 1),
    F(hitCap999, 0, 0, 1),
    F(betaCoin, 0, 0, 1),
    F(betaBombExplosion, 0, 0, 1),
    F(betaBoostGauge, 0, 0, 1),
    F(szMissileBug, 0, 0, 1),
    CP(0), CP(1), CP(2), CP(3), CP(4), CP(5), CP(6), CP(7), CP(8), CP(9), CP(10),
    CP(11), CP(12), CP(13), CP(14), CP(15), CP(16), CP(17), CP(18), CP(19), CP(20),
};

#define NFIELDS ((int) (sizeof(sFields) / sizeof(sFields[0])))

static int* field(XboxSettings* s, const Field* f) {
    return (int*) ((char*) s + f->off);
}

void xbox_settings_defaults(XboxSettings* s) {
    int i;

    memset(s, 0, sizeof(*s));
    for (i = 0; i < NFIELDS; i++) {
        *field(s, &sFields[i]) = sFields[i].def;
    }
}

const char* xbox_vmu_save_dir(void); /* kos_vmu_xbox.c: '\' at the end, or NULL */

static int ini_path(char* out, size_t n) {
    const char* dir = xbox_vmu_save_dir();

    if (dir == NULL) {
        return 0;
    }
    snprintf(out, n, "%ssf64x.ini", dir);
    return 1;
}

void xbox_settings_load(void) {
    char path[300];
    char line[128];
    FILE* fp;
    int i;

    xbox_settings_defaults(&gXboxCfg);
    if (ini_path(path, sizeof(path)) && (fp = fopen(path, "r")) != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            char* eq = strchr(line, '=');

            if (eq == NULL) {
                continue;
            }
            *eq = '\0';
            for (i = 0; i < NFIELDS; i++) {
                if (strcmp(line, sFields[i].key) == 0) {
                    int v = atoi(eq + 1);

                    if (v < sFields[i].min) {
                        v = sFields[i].min;
                    }
                    if (v > sFields[i].max) {
                        v = sFields[i].max;
                    }
                    *field(&gXboxCfg, &sFields[i]) = v;
                    break;
                }
            }
        }
        fclose(fp);
    }
    sSaved = gXboxCfg;
}

void xbox_settings_save(void) {
    char path[300];
    char tmp[310];
    FILE* fp;
    int i;

    if (memcmp(&sSaved, &gXboxCfg, sizeof(gXboxCfg)) == 0) {
        return;
    }
    if (!ini_path(path, sizeof(path))) {
        dbglog(DBG_WARNING, "settings: no save game to keep sf64x.ini in\n");
        return;
    }
    /* written whole beside it, then moved over it: a power cut mid-write
       leaves the old file, not half a new one */
    snprintf(tmp, sizeof(tmp), "%s.new", path);
    fp = fopen(tmp, "w");
    if (fp == NULL) {
        dbglog(DBG_WARNING, "settings: cannot write %s\n", tmp);
        return;
    }
    for (i = 0; i < NFIELDS; i++) {
        fprintf(fp, "%s=%d\n", sFields[i].key, *field(&gXboxCfg, &sFields[i]));
    }
    if (fclose(fp) != 0) {
        remove(tmp);
        return;
    }
    remove(path);
    if (rename(tmp, path) != 0) {
        dbglog(DBG_WARNING, "settings: cannot replace %s\n", path);
        return;
    }
    sSaved = gXboxCfg;
}
