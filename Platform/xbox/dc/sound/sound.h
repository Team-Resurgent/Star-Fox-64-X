/* dc/sound/sound.h -- Xbox shim: KallistiOS's SH-4 side of the AICA.
 *
 * On the Xbox the AICA is emulated in software (Platform/xbox/aica/):
 * 2 MB of sound RAM in main memory, the channel registers, KallistiOS's
 * stock ARM7 firmware compiled for the x86, and a mixer that plays the
 * channels the way the AICA does into one DirectSound stream.
 *
 * Sound-RAM addresses: an address below AICA_RAM_SIZE is an offset into
 * the emulated sound RAM, as on the Dreamcast; snd_mem_malloc returns the
 * HOST address of the block instead, so the sample addresses the SH-4 side
 * puts in its channel commands (src/audio/aica_synth.c) are ones the x86
 * firmware can follow. Every routine that takes a sound-RAM address accepts
 * both forms (aica_host). */
#ifndef XBOX_DC_SOUND_SOUND_H
#define XBOX_DC_SOUND_SOUND_H

#include <kos/cdefs.h>

__BEGIN_DECLS

uint32_t snd_mem_malloc(size_t size);
void snd_mem_free(uint32_t addr);
uint32_t snd_mem_available(void);
int snd_mem_init(uint32_t reserve);
void snd_mem_shutdown(void);
int snd_init(void);
void snd_shutdown(void);
int snd_sh4_to_aica(void *packet, uint32_t size);
void snd_sh4_to_aica_start(void);
void snd_sh4_to_aica_stop(void);
uint16_t snd_get_pos(unsigned int ch);
bool snd_is_playing(unsigned int ch);

__END_DECLS

#endif /* XBOX_DC_SOUND_SOUND_H */
