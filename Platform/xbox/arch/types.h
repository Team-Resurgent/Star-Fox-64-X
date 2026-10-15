/* arch/types.h -- Xbox shim: KallistiOS's short integer names.
 *
 * KOS defines these in arch/types.h; dc/sound/aica_comm.h keys its own
 * fallback typedefs off __ARCH_TYPES_H, so that guard name is kept. */
#ifndef __ARCH_TYPES_H
#define __ARCH_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;

typedef volatile uint8_t  vuint8;
typedef volatile uint16_t vuint16;
typedef volatile uint32_t vuint32;

typedef uint32_t ptr_t;

/* kos/thread.h: thread ids and priorities */
typedef int prio_t;
typedef uint32_t tid_t;

#endif /* __ARCH_TYPES_H */
