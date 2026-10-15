/* dc/maple/vmu.h -- Xbox shim: the memory card's raw block read. The card
 * is the title's save game (Platform/xbox/kos_vmu_xbox.c); block 255 is
 * its root block, which is the only one the port reads directly. */
#ifndef XBOX_DC_MAPLE_VMU_H
#define XBOX_DC_MAPLE_VMU_H

#include <kos/cdefs.h>
#include <dc/maple.h>

__BEGIN_DECLS

int vmu_block_read(maple_device_t *dev, uint16_t blocknum, uint8_t *buffer);
int vmu_block_write(maple_device_t *dev, uint16_t blocknum, const uint8_t *buffer);

__END_DECLS

#endif /* XBOX_DC_MAPLE_VMU_H */
