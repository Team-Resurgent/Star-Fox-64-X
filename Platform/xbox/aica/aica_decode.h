/* aica_decode.h -- one AICA channel's sample stream: the Yamaha ADPCM
 * decoder and the loop, as the chip runs them.
 *
 * Pure C with no Xbox headers.
 *
 * A nibble moves the prediction by (2m + 1) * scale / 8 -- truncated, the
 * sign applied after -- and the scale by {230 x4, 307, 409, 512, 614} / 256,
 * clamped to 127..24576 (the port's encoder, tools/aica/yamaha_adpcm.py, and
 * its own streaming decoder in src/audio/aica_synth.c agree; so does
 * flycast's AICA model). On a loop a plain ADPCM channel goes back to the
 * prediction and scale it had at the loop start, which the chip records the
 * first time it passes it; a long-stream one carries straight on. */
#ifndef XBOX_AICA_DECODE_H
#define XBOX_AICA_DECODE_H

#include <stdint.h>

typedef struct Voice
{
    int keyed;                  /* KYONB, as last executed */
    int playing;                /* producing sound (a one-shot stops at LEA) */
    int fmt;
    int loop;
    uint32_t lsa, lea;
    const uint8_t *data;        /* sample memory */
    uint32_t pos;               /* the sample being played */
    uint32_t frac;              /* 16-bit fraction past it */
    int32_t cur, nxt;           /* samples pos and pos + 1 */
    uint32_t dec;               /* the next sample index the decoder yields */
    int32_t pred, scale;        /* ADPCM decoder state, before sample `dec` */
    int32_t loop_pred, loop_scale;
    int loop_saved;
} Voice;


static const int32_t kAicaAdpcmStep[8] = { 230, 230, 230, 230, 307, 409, 512, 614 };

static inline int32_t decode_next(Voice *v)
{
    int32_t s;
    uint32_t i = v->dec;

    if (v->fmt >= 2)
    {
        int code, m, move;

        /* the chip keeps the decoder's state at the loop start, to come
         * back to it */
        if (i == v->lsa && !v->loop_saved)
        {
            v->loop_pred = v->pred;
            v->loop_scale = v->scale;
            v->loop_saved = 1;
        }
        code = (v->data[i >> 1] >> ((i & 1) ? 4 : 0)) & 0xF;
        m = code & 7;
        move = ((2 * m + 1) * v->scale) >> 3;
        v->pred = (code & 8) ? v->pred - move : v->pred + move;
        if (v->pred > 32767)
            v->pred = 32767;
        else if (v->pred < -32768)
            v->pred = -32768;
        v->scale = (v->scale * kAicaAdpcmStep[m]) >> 8;
        if (v->scale < 127)
            v->scale = 127;
        else if (v->scale > 24576)
            v->scale = 24576;
        s = v->pred;
    }
    else if (v->fmt == 1)
    {
        s = (int32_t)(int8_t)v->data[i] << 8;
    }
    else
    {
        s = ((const int16_t *)v->data)[i];
    }
    v->dec = i + 1;
    /* past the loop end the chip goes back to the loop start; a plain ADPCM
     * channel (PCMS 2) takes back the decoder state it had there, a long
     * stream (PCMS 3) decodes straight on, which is what it is for */
    if (v->dec >= v->lea && v->loop)
    {
        v->dec = v->lsa;
        if (v->fmt == 2 && v->loop_saved)
        {
            v->pred = v->loop_pred;
            v->scale = v->loop_scale;
        }
    }
    return s;
}

#endif /* XBOX_AICA_DECODE_H */
