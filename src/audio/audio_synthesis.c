#include "n64sys.h"
#include "sf64audio_provisional.h"
#undef VIRTUAL_TO_PHYSICAL2
#undef OS_K0_TO_PHYSICAL

#include "mixer.h"

#include "aica_synth.h"

// was 0x470
// now 36*32
#define DMEM_WET_SCRATCH 0x480

// was 0x990
// now 77*32
#define DMEM_COMPRESSED_ADPCM_DATA 0x9A0

// was 0x990
// now 77*32
#define DMEM_LEFT_CH 0x9A0

// was 0xB10
// now 89*32
#define DMEM_RIGHT_CH 0xB20

// was 0x650
// now 51*32
#define DMEM_HAAS_TEMP 0x660

// was 0x450
// now 35*32
#define DMEM_TEMP 0x460

// was 0x5F0
// now 0x600
#define DMEM_UNCOMPRESSED_NOTE 0x600

// was 0xC90
// now 101*32
#define DMEM_WET_LEFT_CH 0xCA0

// was 0xE10
// now 113*32
#define DMEM_WET_RIGHT_CH 0xE20

// = DMEM_WET_LEFT_CH + DMEM_1CH_SIZE
#define SAMPLE_SIZE sizeof(s16)

typedef enum {
    /* 0 */ HAAS_EFFECT_DELAY_NONE,
    /* 1 */ HAAS_EFFECT_DELAY_LEFT, // Delay left channel so that right channel is heard first
    /* 2 */ HAAS_EFFECT_DELAY_RIGHT // Delay right channel so that left channel is heard first
} HaasEffectDelaySide;

s32 D_80145D40; // unused

// all of these are part of the DFT-related function
f32 D_80145D48[256];
f32 D_80146148[256];
f32 D_80146548[515];
f32 D_80146D54;
f32 D_80146D58;
f32 D_80146D5C;
f32 D_80146D60;
f32 D_80146D64;
f32 D_80146D68;
f32 D_80146D6C;
f32 D_80146D70;

void AudioSynth_SyncSampleStates(s32 updateIndex);

/* XBOX: the decomp's matched AudioSynth_HartleyTransform (sonicdcer/sf64
   ed63cd69), for the Options sound test's spectrum analyzer; the Dreamcast
   port stubs it, its AICA leaving no mixed audio to analyse. */
#if defined(TARGET_XBOX)
void AudioSynth_HartleyTransform(f32* x, s32 m, f32* tab) {
    s32 n;
    s32 im, i, j;
    f32 b;
    s32 pont;
    s32 n2, n4, n8, n_tab;
    s32 is, id;
    f32 *px, *py, *pw, *pz;
    f32 *px1, *py1, *pw1, *pz1;
    f32 *pcc1, *pcc3, *pss1, *pss3;
    f32 a, a3;
    f32 temp;

    n = 1 << m;
    n2 = n << 1;
    n_tab = n / 8 - 1;

    switch (n) {
        case 1:
            break;
        case 2:
            temp = (x[0] + x[1]) * 0.707107f;
            x[1] = (x[0] - x[1]) * 0.707107f;
            x[0] = temp;
            break;

        case 4:
            temp = (x[0] + x[2]) / 2.0f;
            x[2] = (x[0] - x[2]) / 2.0f;
            x[0] = temp;

            temp = (x[1] + x[3]) / 2.0f;
            x[3] = (x[1] - x[3]) / 2.0f;
            x[1] = temp;

            temp = x[0] - temp;
            x[0] = x[0] + x[1];
            x[1] = x[2] + x[3];
            x[3] = x[2] - x[3];
            x[2] = temp;
            break;

        default:
            if (n != (s32) *tab) {
                *tab = n;
                pcc1 = tab + 1;
                pss1 = pcc1 + n_tab;
                pcc3 = pss1 + n_tab;
                pss3 = pcc3 + n_tab;
                a = 6.283186f / n;

                for (j = 0; j < n_tab; j++) {
                    *pcc1++ = cosf(a);
                    *pss1++ = sinf(a);
                    *pcc3++ = cosf(3.0f * a);
                    *pss3++ = sinf(3.0f * a);
                    a += 6.283186f / n;
                }
            }

            pont = 1;
            for (im = 0; im < m - 1; im++) {
                is = 1;
                id = n2;
                n2 >>= 1;
                n4 = n2 >> 2;
                n8 = n2 >> 3;
                do {
                    for (i = is - 1; i < n; i = i + id) {
                        px = x + i;
                        py = px + n4;
                        pw = py + n4;
                        pz = pw + n4;

                        D_80146D54 = *px;
                        *px = *pw + D_80146D54;
                        D_80146D58 = *py;
                        *py = *py + *pz;
                        D_80146D5C = *pw;

                        *pw = ((D_80146D54 - D_80146D5C) + D_80146D58) - *pz;
                        *pz = ((D_80146D54 - D_80146D5C) - D_80146D58) + *pz;

                        if (n4 > 1) {
                            px = px + n8;
                            py = px + n4;
                            pw = py + n4;
                            pz = pw + n4;

                            D_80146D54 = *px;
                            *px = *pw + D_80146D54;

                            D_80146D58 = *py;
                            *py = *pz + D_80146D58;

                            *pw = (D_80146D54 - *pw) * 1.414214f;
                            *pz = (D_80146D58 - *pz) * 1.414214f;

                            pcc1 = tab + pont;
                            pss1 = pcc1 + n_tab;
                            pcc3 = pss1 + n_tab;
                            pss3 = pcc3 + n_tab;

                            for (j = 1; j < n8; j++) {
                                px = x + i + j;
                                py = px + n4;
                                pw = py + n4;
                                pz = pw + n4;

                                D_80146D54 = *px;
                                D_80146D58 = *py;
                                D_80146D5C = *pw;
                                D_80146D60 = *pz;

                                px1 = x + i + n4 - j;
                                py1 = px1 + n4;
                                pw1 = py1 + n4;
                                pz1 = pw1 + n4;

                                D_80146D64 = *px1;
                                D_80146D68 = *py1;
                                D_80146D6C = *pw1;
                                D_80146D70 = *pz1;

                                *px = D_80146D54 + D_80146D5C;
                                *py = D_80146D58 + D_80146D60;

                                *pw = ((D_80146D54 - D_80146D5C + D_80146D64 - D_80146D6C) * *pcc1) +
                                      ((D_80146D60 - D_80146D58 + D_80146D68 - D_80146D70) * *pss1);

                                *pz = ((D_80146D54 - D_80146D5C - D_80146D64 + D_80146D6C) * *pcc3) -
                                      ((D_80146D60 - D_80146D58 - D_80146D68 + D_80146D70) * *pss3);

                                *px1 = D_80146D64 + D_80146D6C;
                                *py1 = D_80146D68 + D_80146D70;

                                *pw1 = ((D_80146D54 - D_80146D5C + D_80146D64 - D_80146D6C) * *pss1) -
                                       ((D_80146D60 - D_80146D58 + D_80146D68 - D_80146D70) * *pcc1);

                                *pz1 = ((D_80146D54 - D_80146D5C - D_80146D64 + D_80146D6C) * *pss3) +
                                       ((D_80146D60 - D_80146D58 - D_80146D68 + D_80146D70) * *pcc3);

                                pcc1 += pont;
                                pcc3 += pont;
                                pss1 += pont;
                                pss3 += pont;
                            }
                        }
                    }
                    is = 2 * id - n2 + 1;
                    id = 4 * id;
                } while (is < n);

                pont = 2 * pont;

                px = x;
                for (i = 0; i < n; i++) {
                    *px /= 1.414214f;
                    px++;
                }
            }

            is = 1;
            id = 4;
            do {
                for (i = is - 1; i < n; i += id) {
                    px = x + i;
                    D_80146D54 = *px;
                    *px = D_80146D54 + *(px + 1);
                    *(px + 1) = D_80146D54 - *(px + 1);
                }
                is = 2 * id - 1;
                id = 4 * id;
            } while (is < n);

            px = x;
            for (i = 0; i < n; i++) {
                *px /= 1.414214f;
                px++;
            }

            j = 1;
            pw = x;
            for (i = 1; i < n; i++) {
                pont = n >> 1;
                if (i < j) {
                    py = x + j - 1;
                    D_80146D54 = *py;
                    *py = *pw;
                    *pw = D_80146D54;
                }
                pw++;
                while (pont < j) {
                    j -= pont;
                    pont >>= 1;
                }
                j += pont;
            }
            break;
    }
}
#else
void AudioSynth_HartleyTransform(f32* arg0, s32 arg1, f32* arg2) {
    ;
}
#endif

/**
 * Sync the sample states between the notes and the list
 */
void AudioSynth_SyncSampleStates(s32 updateIndex) {
    NoteSubEu* noteSampleState;
    NoteSubEu* sampleState;
    s32 i;

    for (i = 0; i < gNumNotes; i++) {
        noteSampleState = &gNotes[i].noteSubEu;
        sampleState = &gNoteSubsEu[gNumNotes * updateIndex + i];
        if (noteSampleState->bitField0.enabled) {
            *sampleState = *noteSampleState;
            noteSampleState->bitField0.needsInit = 0;
        } else {
            sampleState->bitField0.enabled = 0;
        }
    }
}
#include <stdio.h>
Acmd* AudioSynth_Update(Acmd* aList, s32* cmdCount, s16* aiBufStartL, s16* aiBufStartR, s32 aiBufLen) {
    s32 i;

    for (i = gAudioBufferParams.ticksPerUpdate; i > 0; i--) {
        AudioSeq_ProcessSequences(i - 1);
        AudioSynth_SyncSampleStates(gAudioBufferParams.ticksPerUpdate - i);
        /* NOTE: sub-tick AicaSynth_RefreshActive() disabled while diagnosing an
           intro hang (suspected SH4->AICA command-queue overflow under the 4x
           burst + streamer). Re-enable once throttled. */
//        AicaSynth_RefreshActive(i-1);
    }

    /* AICA hardware mixing: the sequence tick + sample-state prepass above is all the
       SH4 does; AICA renders the finalized gNoteSubsEu voices in hardware. The N64 RSP
       software render (DoOneAudioUpdate -> ProcessNote -> ...) was dropped. */
    (void) aiBufStartL; (void) aiBufStartR; (void) aiBufLen;
    AicaSynth_Update();
    *cmdCount = 0;
    return aList;
}