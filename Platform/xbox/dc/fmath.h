/* dc/fmath.h -- Xbox shim: KallistiOS's SH-4 fast maths (dc/fmath.h and
 * dc/fmath_base.h, KOS licence), as the SH-4 computes it.
 *
 *   fipr       a 4-component dot product
 *   fsin/fcos  fsca: radians * 10430.37835, truncated (ftrc), low 16 bits =
 *              a binary angle -- so the result is the sine of the QUANTISED
 *              angle, as on the Dreamcast (sh4zam_x86.h shz_sincosf)
 *   fisin...   the same from an integer binary angle
 *   fsincos    degrees * 182.04444443 (65536 / 360), same quantisation
 *   fsqrt      fsqrt; frsqrt = 1 / sqrt (fsrra) */
#ifndef XBOX_DC_FMATH_H
#define XBOX_DC_FMATH_H

#include <kos/cdefs.h>
#include <math.h>
#include "../sh4zam_x86.h"

__BEGIN_DECLS

static inline float fipr(float x, float y, float z, float w, float a, float b, float c, float d) {
    return x * a + y * b + z * c + w * d;
}

static inline float fipr_magnitude_sqr(float x, float y, float z, float w) {
    return x * x + y * y + z * z + w * w;
}

static inline float fsin(float r) {
    return shz_sincosf(r).sin;
}

static inline float fcos(float r) {
    return shz_sincosf(r).cos;
}

static inline float ftan(float r) {
    shz_sincos_t sc = shz_sincosf(r);
    return sc.sin / sc.cos;
}

static inline float fisin(int d) {
    return shz_sincosu16((uint16_t) d).sin;
}

static inline float ficos(int d) {
    return shz_sincosu16((uint16_t) d).cos;
}

static inline float fitan(int d) {
    shz_sincos_t sc = shz_sincosu16((uint16_t) d);
    return sc.sin / sc.cos;
}

static inline float fsqrt(float f) {
    return sqrtf(f);
}

static inline float frsqrt(float f) {
    return 1.0f / sqrtf(f);
}

static inline void fsincos(float f, float *s, float *c) {
    shz_sincos_t sc = shz_sincosu16((uint16_t) shz_ftrc_(f * 182.04444443f));
    *s = sc.sin;
    *c = sc.cos;
}

static inline void fsincosr(float f, float *s, float *c) {
    shz_sincos_t sc = shz_sincosf(f);
    *s = sc.sin;
    *c = sc.cos;
}

__END_DECLS

#endif /* XBOX_DC_FMATH_H */
