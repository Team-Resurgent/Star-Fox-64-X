#include <stdint.h>

#if defined(TARGET_XBOX)
/* XBOX: COFF caps alignment at 8192 bytes; nothing reads the address bits
   the Dreamcast's 16 KB alignment fixes (it lines the buffer up with the SH-4
   operand cache), so the largest COFF allows serves the same purpose. */
uint16_t __attribute__((aligned(8192))) rgba16_buf[64 * 64];
#else
uint16_t __attribute__((aligned(16384))) rgba16_buf[64 * 64];
#endif

/* Used for rescaling textures into pow2 dims */
uint16_t __attribute__((aligned(32))) scaled[64 * 64];

uint16_t __attribute__((aligned(32))) scaled2[256 * 128];