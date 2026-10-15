/* dc/g2bus.h -- Xbox shim: the G2 bus accessors, for the two things on it
 * the port touches -- the AICA's registers (0xa0700000 + reg) and its
 * sound RAM (0xa0800000 + offset, or a host address from snd_mem_malloc).
 * Both are the emulated AICA (Platform/xbox/aica/aica_bus_xbox.c); any
 * other G2 address is a port bug and is reported, not dereferenced. */
#ifndef XBOX_DC_G2BUS_H
#define XBOX_DC_G2BUS_H

#include <kos/cdefs.h>

__BEGIN_DECLS

uint8_t g2_read_8(uintptr_t address);
void g2_write_8(uintptr_t address, uint8_t value);
uint16_t g2_read_16(uintptr_t address);
void g2_write_16(uintptr_t address, uint16_t value);
uint32_t g2_read_32(uintptr_t address);
void g2_write_32(uintptr_t address, uint32_t value);
void g2_read_block_32(uint32_t *output, uintptr_t address, size_t amt);
void g2_write_block_32(const uint32_t *input, uintptr_t address, size_t amt);
void g2_fifo_wait(void);

__END_DECLS

#endif /* XBOX_DC_G2BUS_H */
