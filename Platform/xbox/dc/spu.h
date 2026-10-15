/* dc/spu.h -- Xbox shim: loads into the AICA's sound RAM, which is the
 * emulated one (Platform/xbox/aica/aica_bus_xbox.c; see dc/sound/sound.h
 * on the two address forms). */
#ifndef XBOX_DC_SPU_H
#define XBOX_DC_SPU_H

#include <kos/cdefs.h>

__BEGIN_DECLS

#define SPU_RAM_BASE          0x00800000
/* KOS: MEM_AREA_P2_BASE | SPU_RAM_BASE. Kept as the same number so the
 * port's `SPU_RAM_UNCACHED_BASE + offset` arithmetic is unchanged; the
 * G2 accessors decode it (dc/g2bus.h). */
#define SPU_RAM_UNCACHED_BASE 0xa0800000u

void spu_memload(uintptr_t to, const void *from, size_t length);
void spu_memload_sq(uintptr_t to, const void *from, size_t length);
void spu_memload_dma(uintptr_t to, const void *from, size_t length);
void spu_memread(void *to, uintptr_t from, size_t length);
void spu_memset(uintptr_t to, uint32_t what, size_t length);
void spu_memset_sq(uintptr_t to, uint32_t what, size_t length);
void spu_enable(void);
void spu_disable(void);
void spu_master_mixer(int volume, int stereo);
int spu_init(void);
int spu_shutdown(void);
void spu_reset_chans(void);

__END_DECLS

#endif /* XBOX_DC_SPU_H */
