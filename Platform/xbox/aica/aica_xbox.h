/* aica_xbox.h -- the software AICA, shared by Platform/xbox/aica/.
 *
 * What the Dreamcast's sound chip is to the port, emulated far enough to
 * run KallistiOS's stock firmware and play what it plays:
 *
 *   sound RAM   2 MB in main memory (xbox_aica_ram). An AICA address below
 *               2 MB is an offset into it, as on the Dreamcast; snd_mem_malloc
 *               hands out HOST addresses of blocks in it instead, so the
 *               sample addresses the SH-4 side puts in its channel commands
 *               (src/audio/aica_synth.c) are ones the x86 firmware can
 *               follow. aica_host() takes either.
 *   registers   the 32 KB register file (xbox_aica_regs): channel registers
 *               at 0, common registers at 0x2800. Written by KOS's channel
 *               routines, read by the mixer.
 *   ARM7        KOS's firmware loop (arm/main.c) run on the x86, one pass
 *               every 100 output samples -- the 2.27 ms its timer_wait(10)
 *               paces it to on the Dreamcast (aica_fw_xbox.c).
 *   channels    64, Yamaha ADPCM (plain and long-stream) / PCM16 / PCM8,
 *               with the AICA's pitch, level and pan registers and its ADPCM
 *               decoder (aica_mixer_xbox.c).
 *
 * The mix goes to one DirectSound stream at 44.1 kHz, the AICA's rate. */
#ifndef XBOX_AICA_XBOX_H
#define XBOX_AICA_XBOX_H

#include <stdint.h>

#define AICA_RAM_SIZE   0x200000u
#define AICA_RATE       44100
#define AICA_PASS       100         /* samples per firmware pass: 10 ticks of 4410 Hz */

extern uint8_t *xbox_aica_ram;
extern volatile unsigned char xbox_aica_regs[0x8000];

/* A sound-RAM address in either form -> a host pointer. */
static inline void *aica_host(uint32_t a)
{
    return (a < AICA_RAM_SIZE) ? (void *)(xbox_aica_ram + a) : (void *)(uintptr_t)a;
}

/* aica_fw_xbox.c */
void aica_fw_boot(void);
void aica_fw_pass(void);
extern int aica_fw_running;

/* aica_mixer_xbox.c */
int aica_mixer_start(void);
/* the latest mix, resampled to `rate`, `frames` interleaved stereo frames
 * (src/audio/audio_thread.c: the sound test's spectrum analyzer) */
void aica_mixer_tap(int16_t *dst, int frames, int rate);
void aica_mixer_key_on(int ch);
void aica_mixer_key_off(int ch);
uint32_t aica_mixer_pos(int ch);
/* the full sample address of a channel: KOS's register encoding has 23
 * bits for it, and a host address does not fit (aica_fw_xbox.c aica_play) */
extern uint32_t xbox_aica_chan_base[64];

#endif /* XBOX_AICA_XBOX_H */
