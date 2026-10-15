/* aica.h -- Xbox shim for KallistiOS's ARM-side aica.h (kernel/arch/
 * dreamcast/sound/arm/aica.h): the channel register macros and the channel
 * routines. On the Xbox the registers are the emulated AICA's register
 * file, which the mixer reads (Platform/xbox/aica/). The routines are
 * KOS's, re-expressed in Platform/xbox/aica/aica_fw_xbox.c. */
#ifndef __AICA_H
#define __AICA_H

#include <arch/types.h>

extern volatile unsigned char xbox_aica_regs[0x8000];

#define dc_snd_base (xbox_aica_regs)

#define SNDREG32A(x) ((volatile unsigned long *)(dc_snd_base + (x)))
#define SNDREG32(x) (*SNDREG32A(x))
#define SNDREG8A(x) (dc_snd_base + (x))
#define SNDREG8(x) (*SNDREG8A(x))
#define CHNREG32A(chn, x) SNDREG32A(0x80*(chn) + (x))
#define CHNREG32(chn, x) (*CHNREG32A(chn, x))
#define CHNREG8A(chn, x) SNDREG8A(0x80*(chn) + (x))
#define CHNREG8(chn, x) (*CHNREG8A(chn, x))

void aica_init(void);
void aica_play(int ch, int delay);
void aica_sync_play(uint32 chmap);
void aica_stop(int ch);
void aica_vol(int ch);
void aica_pan(int ch);
void aica_freq(int ch);
int aica_get_pos(int ch);

#endif /* __AICA_H */
