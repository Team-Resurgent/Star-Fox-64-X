/* xbox_interp.c -- 60 fps by frame interpolation (the port's fps60 option).
 *
 * The game runs its logic at 30 fps (gVIsPerFrame 2; 3 on Aquas, more in
 * some cutscenes), and that stays as it is. What changes is the drawing:
 * each logic frame's display list is drawn gVIsPerFrame times, one
 * vertical blank apiece, the last time as built and the earlier times with
 * every model-view matrix blended between the previous logic frame's and
 * this one's -- so the picture moves every 1/60 s and the game plays at
 * its own speed. The method is Starship's (src/port/interpolation), built
 * on what this port's renderer offers:
 *
 *   - every model-view matrix the game writes goes through Matrix_ToMtx
 *     into the frame's matrix pool (gGfxPool->mtx); each is recorded with a
 *     key: the object tag it was drawn under (XboxInterp_Push/Pop, around
 *     the game's object, player, effect and shot draws) and its order
 *     within that tag, so an object is blended with itself even when
 *     others appear or vanish around it;
 *   - after the frame is built, each matrix finds last frame's of the same
 *     key; while drawing a blended pass the interpreter
 *     (src/gfx/gfx_retro_dc.c) loads XboxInterp_Lookup's blend in place of
 *     the pool's matrix;
 *   - a matrix whose change from last frame is a jump -- a camera cut, an
 *     object wrapping or respawning, a fast spin -- is not blended, it is
 *     shown as built;
 *   - the projection, 2D (HUD rectangles, the starfield's points) and
 *     anything else not drawn through a pool matrix stay at 30 fps.
 *
 * The cost is drawing the frame more than once: when the frame's logic and
 * draws would not fit in the time the logic frame has, the port goes back
 * to one draw per logic frame for a second before trying again.
 */
#include "global.h"

#define POOL_MTX ((s32) ARRAY_COUNT(((GfxPool*) 0)->mtx))
#define HASH_SIZE 4096 /* power of two, > 2 * POOL_MTX */
#define TAG_DEPTH 24

typedef struct {
    u32 key;
    s16 slot;
    s16 match; /* index of last frame's record of this key, or -1 */
    float m[16];
} Rec;

static Rec sRec[2][0x480];
static s32 sRecCount[2];
static s32 sCur;               /* the frame being recorded */
static const Mtx* sPoolBase;   /* its pool */
static s32 sRecording;

static u32 sKey[TAG_DEPTH];
static u32 sOrd[TAG_DEPTH];
static s32 sDepth;

static u32 sHashKey[HASH_SIZE];
static s16 sHashIdx[HASH_SIZE];

static float sRepl[0x480][16] __attribute__((aligned(16)));
static u8 sReplValid[0x480];
static s32 sReplOn;

static s32 sCooldown;         /* logic frames left at one draw each */

/* the starfield, per pane (split screen draws one each): its field offset
   per frame (index as sRec), its roll this frame, and the blended draw's
   offset */
static float sStarField[2][4][2];
static s32 sStarHave[2][4];
static float sStarCos[4] = { 1.0f, 1.0f, 1.0f, 1.0f }, sStarSin[4];
static float sStarD[4][2];

int gXboxDrawVIs;

/* for the frame-time readout: matrices recorded / blended on the last blended draw */
static s32 sStatRecorded, sStatBlended;

void XboxInterp_Stats(int* recorded, int* blended) {
    *recorded = sStatRecorded;
    *blended = sStatBlended;
}

static u32 mix(u32 h, u32 v) {
    h ^= v + 0x9E3779B9u + (h << 6) + (h >> 2);
    h *= 0x85EBCA6Bu;
    return h ^ (h >> 13);
}

void XboxInterp_FrameBegin(void) {
    sCur ^= 1;
    sRecCount[sCur] = 0;
    sDepth = 0;
    sKey[0] = 0x5F3759DFu;
    sOrd[0] = 0;
    sPoolBase = gGfxPool->mtx;
    sRecording = XBOX_CFG(fps60);
    sReplOn = false;
    memset(sStarHave[sCur], 0, sizeof(sStarHave[sCur]));
    memset(sStarD, 0, sizeof(sStarD));
}

/* the pane being drawn is gPlayerNum's (Game_Draw) */
void XboxInterp_Starfield(float xField, float yField, float rollCos, float rollSin) {
    s32 p = gPlayerNum & 3;

    sStarField[sCur][p][0] = xField;
    sStarField[sCur][p][1] = yField;
    sStarHave[sCur][p] = true;
    sStarCos[p] = rollCos;
    sStarSin[p] = rollSin;
}

void XboxInterp_StarOffset(int pane, float* dx, float* dy) {
    *dx = sStarD[pane & 3][0];
    *dy = sStarD[pane & 3][1];
}

void XboxInterp_Push(const void* ptr, int id) {
    if (sDepth + 1 >= TAG_DEPTH) {
        sDepth++; /* too deep: matrices go on the deepest tag kept */
        return;
    }
    sKey[sDepth + 1] = mix(mix(sKey[sDepth], (u32) (uintptr_t) ptr), (u32) id);
    sOrd[sDepth + 1] = 0;
    sDepth++;
}

void XboxInterp_Pop(void) {
    if (sDepth > 0) {
        sDepth--;
    }
}

/* From Matrix_ToMtx: `dest` is about to hold `mf` (the float matrix the
   game's GBI_FLOATS Mtx carries). */
void XboxInterp_Record(const Mtx* dest, const f32 mf[4][4]) {
    s32 slot, d;
    Rec* r;

    if (!sRecording) {
        return;
    }
    slot = (s32) (dest - sPoolBase);
    if ((slot < 0) || (slot >= POOL_MTX) || (sRecCount[sCur] >= POOL_MTX)) {
        return;
    }
    d = (sDepth < TAG_DEPTH) ? sDepth : (TAG_DEPTH - 1);
    r = &sRec[sCur][sRecCount[sCur]++];
    r->key = mix(sKey[d], sOrd[d]++);
    r->slot = (s16) slot;
    r->match = -1;
    memcpy(r->m, mf, sizeof(r->m));
}

/* The frame is built: find each matrix's predecessor. */
void XboxInterp_FrameEnd(void) {
    s32 prev = sCur ^ 1;
    s32 i;

    sRecording = false;
    if (!XBOX_CFG(fps60)) {
        return;
    }
    memset(sHashIdx, 0xFF, sizeof(sHashIdx));
    for (i = 0; i < sRecCount[prev]; i++) {
        u32 h = sRec[prev][i].key & (HASH_SIZE - 1);

        while (sHashIdx[h] >= 0) {
            h = (h + 1) & (HASH_SIZE - 1);
        }
        sHashKey[h] = sRec[prev][i].key;
        sHashIdx[h] = (s16) i;
    }
    for (i = 0; i < sRecCount[sCur]; i++) {
        Rec* r = &sRec[sCur][i];
        u32 h = r->key & (HASH_SIZE - 1);

        while (sHashIdx[h] >= 0) {
            if (sHashKey[h] == r->key) {
                r->match = sHashIdx[h];
                break;
            }
            h = (h + 1) & (HASH_SIZE - 1);
        }
    }
}

/* Blendable: the rotation/scale part moved less than a third of its size
   and the position less than a jump (scaled by how far away it is). */
static bool blendable(const float* a, const float* b) {
    float scale = 0.0f, diff = 0.0f, dx, dy, dz, dist2, far2;
    s32 i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            float va = fabsf(a[i * 4 + j]), d = fabsf(a[i * 4 + j] - b[i * 4 + j]);

            if (va > scale) {
                scale = va;
            }
            if (d > diff) {
                diff = d;
            }
        }
    }
    if (diff > scale * 0.35f) {
        return false;
    }
    dx = a[12] - b[12];
    dy = a[13] - b[13];
    dz = a[14] - b[14];
    dist2 = dx * dx + dy * dy + dz * dz;
    far2 = b[12] * b[12] + b[13] * b[13] + b[14] * b[14];
    /* (300 + far/4)^2, without the root: at most 2 * (300^2 + far^2/16) */
    return dist2 < 2.0f * (90000.0f + far2 * (1.0f / 16.0f));
}

/* Before each draw of the frame: pass k of n (k == n: the frame as built). */
void XboxInterp_SetStep(int k, int n) {
    s32 prev = sCur ^ 1;
    float t, u;
    s32 i, j;

    if ((k >= n) || !XBOX_CFG(fps60)) {
        sReplOn = false;
        memset(sStarD, 0, sizeof(sStarD));
        return;
    }
    t = (float) k / (float) n;
    u = 1.0f - t;

    /* the stars: a point sits at R(roll) * (star + field) on screen, so the
       field's move since last frame moves them all alike; back by (1-t) of
       it. A wrap of the field (480 x 360) is a jump, not a move. */
    memset(sStarD, 0, sizeof(sStarD));
    for (j = 0; j < 4; j++) {
        if (sStarHave[sCur][j] && sStarHave[prev][j]) {
            float fx = sStarField[prev][j][0] - sStarField[sCur][j][0];
            float fy = sStarField[prev][j][1] - sStarField[sCur][j][1];

            if ((fabsf(fx) < 100.0f) && (fabsf(fy) < 100.0f)) {
                sStarD[j][0] = u * (sStarCos[j] * fx + sStarSin[j] * fy);
                sStarD[j][1] = u * (-sStarSin[j] * fx + sStarCos[j] * fy);
            }
        }
    }
    memset(sReplValid, 0, sizeof(sReplValid));
    sStatRecorded = sRecCount[sCur];
    sStatBlended = 0;
    for (i = 0; i < sRecCount[sCur]; i++) {
        const Rec* r = &sRec[sCur][i];
        const float* a;
        float* o;

        if (r->match < 0) {
            continue;
        }
        a = sRec[prev][r->match].m;
        if (!blendable(a, r->m)) {
            continue;
        }
        o = sRepl[r->slot];
        for (j = 0; j < 16; j++) {
            o[j] = a[j] * u + r->m[j] * t;
        }
        sReplValid[r->slot] = 1;
        sStatBlended++;
    }
    sReplOn = true;
}

/* The interpreter, loading a model-view matrix: the blend to use instead,
   or NULL. */
const float* XboxInterp_Lookup(const void* addr) {
    s32 slot;

    if (!sReplOn) {
        return NULL;
    }
    slot = (s32) ((const Mtx*) addr - sPoolBase);
    if ((slot < 0) || (slot >= POOL_MTX) || !sReplValid[slot]) {
        return NULL;
    }
    return sRepl[slot];
}

/* How many times to draw this logic frame: once, or once per vertical
   blank it lasts -- unless the last frames did not fit. `busyUs` is how
   long the previous logic frame's logic and draws took, `draws` how many
   draws that was. */
int XboxInterp_Draws(unsigned int busyUs, int draws) {
    s32 n = gVIsPerFrame;

    if (!XBOX_CFG(fps60) || (n < 2)) {
        sCooldown = 0;
        return 1;
    }
    if (n > 5) {
        n = 5;
    }
    /* the time it has is `draws` blanks of 16.68 ms; keep a little back */
    if ((draws > 1) && (busyUs > (u32) draws * 15800u)) {
        sCooldown = 60;
    }
    if (sCooldown > 0) {
        sCooldown--;
        return 1;
    }
    return n;
}
