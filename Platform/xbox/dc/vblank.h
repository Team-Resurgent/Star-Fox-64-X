/* dc/vblank.h -- Xbox shim: KallistiOS's vertical-blank handlers.
 *
 * On the Dreamcast these run from the PVR's vblank interrupt. Here a
 * time-critical thread waits for each of the NV2A's vertical blanks
 * (D3DDevice_BlockUntilVerticalBlank, once the PVR is up; a 60 Hz clock
 * before that) and calls them, in the order they were added.
 * (Platform/xbox/kos_extra_xbox.c) */
#ifndef XBOX_DC_VBLANK_H
#define XBOX_DC_VBLANK_H

#include <kos/cdefs.h>

__BEGIN_DECLS

typedef void (*vblank_handler_t)(uint32_t code, void *data);

/* the ASIC event code KOS hands its handlers */
#define ASIC_EVT_PVR_VBLANK_BEGIN 0x0003

int vblank_handler_add(vblank_handler_t hnd, void *data);
int vblank_handler_remove(int handle);

__END_DECLS

#endif /* XBOX_DC_VBLANK_H */
