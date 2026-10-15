/* kos/cdefs.h -- Xbox shim for KallistiOS's compiler-definitions header.
 *
 * Only what the port's sources and the other shim headers use. Platform/xbox
 * is first on the include path, so <kos/...> resolves here, never to a real
 * KallistiOS tree (there is none on this build machine).
 */
#ifndef XBOX_KOS_CDEFS_H
#define XBOX_KOS_CDEFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* (the build defines both empty for C: tools/xbox/xbconfig.py DEFINES) */
#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

#ifndef __noreturn
#define __noreturn __attribute__((__noreturn__))
#endif
#ifndef __nonnull_all
#define __nonnull_all
#endif
#ifndef __depr
#define __depr(m) __attribute__((deprecated(m)))
#endif
#ifndef __RESTRICT
#define __RESTRICT __restrict
#endif
#ifndef __unused
#define __unused __attribute__((__unused__))
#endif
#ifndef __likely
#define __likely(x) __builtin_expect(!!(x), 1)
#define __unlikely(x) __builtin_expect(!!(x), 0)
#endif

/* kos/regfield.h */
#ifndef BIT
#define BIT(bit) (1u << (bit))
#endif
#ifndef GENMASK
#define GENMASK(h, l) ((0xffffffffu << (l)) & (0xffffffffu >> (31 - (h))))
#endif
#ifndef FIELD_GET
#define FIELD_GET(var, field) (((var) & (field)) >> __builtin_ctz(field))
#define FIELD_PREP(field, value) (((value) << __builtin_ctz(field)) & (field))
#endif

#endif /* XBOX_KOS_CDEFS_H */
