/* dc/sq.h -- Xbox shim: the SH-4 store queues.
 *
 * The Star Fox renderer (src/gfx/gfx_pvr.c) ships its vertices to the PVR's
 * tile accelerator with sq_fast_cpy(SQ_MASK_DEST(PVR_TA_INPUT), ...): 32-byte
 * blocks through the store queues into the TA FIFO. Here that destination
 * is recognised and the blocks go to the emulated TA (pvr_prim,
 * Platform/xbox/pvr_xbox.c), which parses the same words. The addresses
 * keep KOS's values so the arithmetic in the port is unchanged. */
#ifndef XBOX_DC_SQ_H
#define XBOX_DC_SQ_H

#include <kos/cdefs.h>

__BEGIN_DECLS

#define MEM_AREA_SQ_BASE 0xe0000000u

/* KOS: the store-queue area address that writes through to `dest` */
#define SQ_MASK_DEST(dest) \
    ((uint32_t *)(void *)(uintptr_t)(MEM_AREA_SQ_BASE | ((uintptr_t)(dest) & 0x03ffffe0u)))

void *sq_set32(void *dest, uint32_t c, size_t n);
void *sq_cpy(void *dest, const void *src, size_t n);
/* `cnt` 32-byte blocks from `src` to the store-queue destination `dest`;
 * the TA input is the one destination the port uses */
void *sq_fast_cpy(void *dest, const void *src, size_t cnt);

__END_DECLS

#endif /* XBOX_DC_SQ_H */
