/* sh4zam_x86.h -- the SH4ZAM maths the Dreamcast port uses, for the Xbox's
 * Pentium III.
 *
 * include/sh4zam.h is SH-4 assembly throughout (fipr, ftrv, fsca, fsrra and
 * the XMTRX register bank). Under TARGET_XBOX it includes this file instead
 * and nothing else changes: every name, type and layout below is the
 * original's, and every routine computes what the SH-4 sequence computes.
 *
 * The conventions that matter, read off the SH-4 code:
 *
 *   XMTRX     sixteen floats, XF0..XF15. Loading a shz_matrix_4x4_t puts
 *             elem[i] in XFi, so elem[0..3] is the first COLUMN.
 *   ftrv      out = XMTRX * v: out[r] = sum over c of XF[4c + r] * v[c].
 *   apply     XMTRX = XMTRX * M (each column of M goes through ftrv and the
 *             results become XMTRX's columns).
 *   fsca      the angle is a 16-bit binary angle (65536 = 2 pi): radians are
 *             scaled by SHZ_FSCA_RAD_FACTOR, TRUNCATED to an integer (ftrc)
 *             and only its low 16 bits are used, so angles wrap. The sine and
 *             cosine here are of that quantised angle.
 *   fsrra     1/sqrt(x), approximate on the SH-4; exact here.
 *
 * XMTRX is one per CPU on the Dreamcast and is saved with a thread's
 * context. Here it is one global: every XMTRX user is on the game thread
 * (sys_matrix.c, gfx_retro_dc.c, the overlays). The only other one is the
 * RSP audio microcode in src/audio/mixer.c, which the AICA build never runs
 * (AudioSynth_Update hands the notes to the AICA instead). RXDK builds with
 * -femulated-tls, so a thread-local here would cost a call on every access
 * in the matrix code. */
#ifndef SH4ZAM_X86_H
#define SH4ZAM_X86_H

#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <xmmintrin.h>

#ifndef true
#define true 1
#define false 0
#endif
#define SHZ_NOEXCEPT
#define SHZ_ALIGNAS(n) __attribute__((aligned(n)))
#define SHZ_INLINE inline static
#define SHZ_FORCE_INLINE __attribute__((always_inline)) SHZ_INLINE
#define SHZ_NO_INLINE __attribute__((noinline))
#define SHZ_ALIASING __attribute__((__may_alias__))
#ifdef __cplusplus
#define SHZ_RESTRICT __restrict__
#else
#define SHZ_RESTRICT restrict
#endif

/* sqrtf as the one SSE instruction. The SDK's libc (picolibc) has a
 * software sqrtf that builds the root a bit at a time, and RXDK compiles
 * with -fno-builtin, so every sqrtf is a call to it -- and the renderer
 * takes one for each vertex's 1/w (fsrra on the SH-4). sqrtss gives the
 * same, correctly rounded, result. Platform/xbox/sh4zam_x86.c makes it
 * the port's sqrtf too. */
SHZ_FORCE_INLINE float shz_sqrtss_(float x) {
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(x)));
}

#define SHZ_FSCA_RAD_FACTOR 10430.37835f

#define SHZ_F_PI 3.1415926f
#define SHZ_F_PI_2 1.57079633f
#define SHZ_F_PI_4 0.78539816f
#define TRIG_ARG_SCALE 0.00009587f
#define SHZ_ANGLE(a) (((float) ((uint16_t) a)) * TRIG_ARG_SCALE)

/* fschg switches the SH-4 FPU's move width; there is nothing to switch here */
#define SHZ_FSCHG(pairwise_mode) do { (void) (pairwise_mode); } while (0)
#define SHZ_LIKELY(e) __builtin_expect(!!(e), 1)
#define SHZ_UNLIKELY(e) __builtin_expect(!!(e), 0)
#define SHZ_PREFETCH(a) __builtin_prefetch(a)

#ifdef __cplusplus
extern "C" {
#endif

typedef SHZ_ALIASING int16_t shz_alias_int16_t;
typedef SHZ_ALIASING uint16_t shz_alias_uint16_t;
typedef SHZ_ALIASING int32_t shz_alias_int32_t;
typedef SHZ_ALIASING uint32_t shz_alias_uint32_t;
typedef SHZ_ALIASING float shz_alias_float_t;
typedef SHZ_ALIASING int64_t shz_alias_int64_t;
typedef SHZ_ALIASING uint64_t shz_alias_uint64_t;
typedef SHZ_ALIASING double shz_alias_double_t;

typedef struct shz_sincos {
    float sin;
    float cos;
} shz_sincos_t;

typedef struct shz_vec2 {
    union {
        float e[2];
        struct {
            float x;
            float y;
        };
    };
} shz_vec2_t;

typedef struct shz_vec3 {
    union {
        float e[3];
        struct {
            union {
                struct {
                    float x;
                    float y;
                };
                shz_vec2_t xy;
            };
            float z;
        };
    };
} shz_vec3_t;

typedef struct shz_vec4 {
    union {
        float e[4];
        struct {
            union {
                struct {
                    float x;
                    float y;
                    float z;
                };
                shz_vec3_t xyz;
            };
            float w;
        };
        struct {
            shz_vec2_t xy;
            shz_vec2_t zw;
        };
    };
} shz_vec4_t;

typedef SHZ_ALIGNAS(8) union shz_matrix_2x2 {
    float elem[4];
    float elem2D[2][2];
    shz_vec2_t col[2];
} shz_matrix_2x2_t;

typedef union shz_matrix_3x3 {
    float elem[9];
    float elem2D[3][3];
    shz_vec3_t col[3];
    struct {
        shz_vec3_t left;
        shz_vec3_t up;
        shz_vec3_t forward;
    };
} shz_matrix_3x3_t;

typedef union shz_matrix_3x4 {
    float elem[12];
    float elem2D[3][4];
    shz_vec3_t col[4];
    struct {
        shz_vec3_t left;
        shz_vec3_t up;
        shz_vec3_t forward;
        shz_vec3_t pos;
    };
} shz_matrix_3x4_t;

typedef SHZ_ALIGNAS(8) union shz_matrix_4x4 {
    float elem[16];
    float elem2D[4][4];
    shz_vec4_t col[4];
    struct {
        shz_vec4_t left;
        shz_vec4_t up;
        shz_vec4_t forward;
        shz_vec4_t pos;
    };
} shz_matrix_4x4_t;

/* ============================================================== XMTRX */

/* Defined once, in Platform/xbox/sh4zam_x86.c. */
extern float shz_xmtrx_[16];
#define SHZ_X shz_xmtrx_

/* out = XMTRX * v */
SHZ_FORCE_INLINE void shz_ftrv_(const float v[4], float out[4]) {
    const float x = v[0], y = v[1], z = v[2], w = v[3];
    out[0] = SHZ_X[0] * x + SHZ_X[4] * y + SHZ_X[8] * z + SHZ_X[12] * w;
    out[1] = SHZ_X[1] * x + SHZ_X[5] * y + SHZ_X[9] * z + SHZ_X[13] * w;
    out[2] = SHZ_X[2] * x + SHZ_X[6] * y + SHZ_X[10] * z + SHZ_X[14] * w;
    out[3] = SHZ_X[3] * x + SHZ_X[7] * y + SHZ_X[11] * z + SHZ_X[15] * w;
}

/* XMTRX = XMTRX * M, M column-major */
SHZ_FORCE_INLINE void shz_xmtrx_mul_(const float m[16]) {
    float r[16];
    shz_ftrv_(&m[0], &r[0]);
    shz_ftrv_(&m[4], &r[4]);
    shz_ftrv_(&m[8], &r[8]);
    shz_ftrv_(&m[12], &r[12]);
    memcpy(SHZ_X, r, sizeof(r));
}

/* ============================================================ scalars */

SHZ_FORCE_INLINE float shz_fmaf(float a, float b, float c) SHZ_NOEXCEPT {
    return a * b + c;
}

SHZ_FORCE_INLINE float shz_copysignf(float x, float y) SHZ_NOEXCEPT {
    x = __builtin_fabsf(x);
    return (y < 0.0f) ? -x : x;
}

/* fsrra: 1/sqrt(x) (fsrra of 0 is +inf, as 1.0f/0.0f is) */
SHZ_FORCE_INLINE float shz_inv_sqrtf(float x) SHZ_NOEXCEPT {
    return 1.0f / shz_sqrtss_(x);
}

SHZ_FORCE_INLINE float shz_invf_fsrra(float x) SHZ_NOEXCEPT {
    return shz_inv_sqrtf(x * x);
}

SHZ_FORCE_INLINE float shz_sqrtf_fsrra(float x) {
    return x == 0.0f ? 0.0f : shz_inv_sqrtf(x) * x;
}

SHZ_FORCE_INLINE float shz_mag_sqr3f(float x, float y, float z) SHZ_NOEXCEPT {
    return x * x + y * y + z * z;
}

SHZ_FORCE_INLINE float shz_mag_sqr4f(float x, float y, float z, float w) {
    return x * x + y * y + z * z + w * w;
}

SHZ_FORCE_INLINE float shz_dot6f(float x1, float y1, float z1, float x2, float y2, float z2) {
    return x1 * x2 + y1 * y2 + z1 * z2;
}

SHZ_FORCE_INLINE float shz_dot8f(float x1, float y1, float z1, float w1, float x2, float y2, float z2, float w2) {
    return x1 * x2 + y1 * y2 + z1 * z2 + w1 * w2;
}

SHZ_FORCE_INLINE float shz_inverse_sqrtf(float x) {
    if (x == 0.0f)
        return 0.0f;
    return 1.0f / shz_sqrtss_(x);
}

/* fsrra(x * x): 1/|x| */
SHZ_FORCE_INLINE float shz_inverse_posf(float x) {
    return shz_inverse_sqrtf(x * x);
}

SHZ_FORCE_INLINE float shz_div_posf(float num, float denom) {
    return num * shz_inverse_posf(denom);
}

SHZ_FORCE_INLINE float shz_fast_invf(float x) {
    float inv;

    if (__builtin_constant_p(x))
        return 1.0f / x;
    inv = shz_inverse_posf(x);
    if (x < 0.0f)
        inv = -inv;
    return inv;
}

SHZ_FORCE_INLINE float shz_divf(float num, float denom) {
    return num * shz_fast_invf(denom);
}

SHZ_FORCE_INLINE float shz_atanf_unit(float x) SHZ_NOEXCEPT {
    const float n1 = 0.97239411f;
    const float n2 = -0.19194795f;

    return shz_fmaf(n2, x * x, n1) * x;
}

SHZ_INLINE float shz_atanf_q1(float x) SHZ_NOEXCEPT {
    return (SHZ_F_PI_2) - shz_atanf_unit(shz_invf_fsrra(x));
}

SHZ_INLINE float shz_atanf(float x) SHZ_NOEXCEPT {
    if (x > 1.0f)
        return shz_atanf_q1(x);
    else if (x < -1.0f)
        return -shz_atanf_q1(x);
    else
        return shz_atanf_unit(x);
}

SHZ_INLINE float shz_atan2f(float y, float x) SHZ_NOEXCEPT {
    float abs_y, abs_sum, inv_abs_sum, angle, r;

    if (__builtin_expect((x == 0.0f && y == 0.0f), 0))
        return 0.0f;
    abs_y = __builtin_fabsf(y);
    abs_sum = abs_y + __builtin_fabsf(x);
    inv_abs_sum = shz_invf_fsrra(abs_sum);
    angle = SHZ_F_PI_2;
    r = x;
    if (x < 0.0f) {
        angle += SHZ_F_PI_4;
        r += abs_y;
    } else {
        angle -= SHZ_F_PI_4;
        r -= abs_y;
    }
    r *= inv_abs_sum;
    angle += shz_fmaf(0.1963f, r * r, -0.9817f) * r;
    return shz_copysignf(angle, y);
}

SHZ_INLINE float shz_asinf(float x) SHZ_NOEXCEPT {
    return shz_atanf(x * shz_inv_sqrtf(1.0f - (x * x)));
}

SHZ_INLINE float shz_acosf(float x) SHZ_NOEXCEPT {
    return (SHZ_F_PI_2) - shz_asinf(x);
}

/* =============================================================== trig */

/* ftrc: float to int, truncating toward zero, saturating */
SHZ_FORCE_INLINE int32_t shz_ftrc_(float f) {
    if (!(f == f))
        return 0;
    if (f >= 2147483647.0f)
        return 2147483647;
    if (f <= -2147483648.0f)
        return (int32_t) 0x80000000u;
    return (int32_t) f;
}

/* fsca: sine and cosine of a 16-bit binary angle */
SHZ_FORCE_INLINE shz_sincos_t shz_sincosu16(uint16_t radians16) {
    const float a = (float) radians16 * (6.28318530717958647692f / 65536.0f);
    shz_sincos_t r;

    r.sin = sinf(a);
    r.cos = cosf(a);
    return r;
}

/* The SH-4 build converts with ftrc and keeps the low 16 bits; done the same
 * way here (a float to uint16_t conversion of a negative value is undefined
 * in C, and the angles here are often negative). */
SHZ_FORCE_INLINE shz_sincos_t shz_sincosf(float radians) {
    return shz_sincosu16((uint16_t) shz_ftrc_(radians * SHZ_FSCA_RAD_FACTOR));
}

SHZ_FORCE_INLINE float shz_sincos_tanf(shz_sincos_t sincos) {
    return sincos.sin / sincos.cos;
}

SHZ_FORCE_INLINE float shz_sinf(float radians) {
    return shz_sincosf(radians).sin;
}

SHZ_FORCE_INLINE float shz_cosf(float radians) {
    return shz_sincosf(radians).cos;
}

SHZ_FORCE_INLINE float shz_tanf(float radians) {
    return shz_sincos_tanf(shz_sincosf(radians));
}

/* ============================================================ vectors */

SHZ_FORCE_INLINE shz_vec3_t shz_vec3_scale(shz_vec3_t vec, float factor) SHZ_NOEXCEPT {
    shz_vec3_t r;
    r.x = vec.x * factor;
    r.y = vec.y * factor;
    r.z = vec.z * factor;
    return r;
}

SHZ_FORCE_INLINE shz_vec2_t shz_vec2_init(float x, float y) SHZ_NOEXCEPT {
    shz_vec2_t r;
    r.x = x;
    r.y = y;
    return r;
}

SHZ_FORCE_INLINE shz_vec3_t shz_vec3_init(float x, float y, float z) SHZ_NOEXCEPT {
    shz_vec3_t r;
    r.x = x;
    r.y = y;
    r.z = z;
    return r;
}

SHZ_FORCE_INLINE shz_vec4_t shz_vec4_init(float x, float y, float z, float w) SHZ_NOEXCEPT {
    shz_vec4_t r;
    r.x = x;
    r.y = y;
    r.z = z;
    r.w = w;
    return r;
}

SHZ_FORCE_INLINE shz_vec2_t shz_vec2_fill(float v) SHZ_NOEXCEPT {
    return shz_vec2_init(v, v);
}

SHZ_FORCE_INLINE float shz_vec3_magnitude_sqr(shz_vec3_t vec) SHZ_NOEXCEPT {
    return shz_mag_sqr4f(vec.x, vec.y, vec.z, 0.0f);
}

SHZ_FORCE_INLINE float shz_vec3_magnitude_inv(shz_vec3_t vec) SHZ_NOEXCEPT {
    return shz_inverse_sqrtf(shz_vec3_magnitude_sqr(vec));
}

SHZ_FORCE_INLINE shz_vec3_t shz_vec3_normalize(shz_vec3_t vec) SHZ_NOEXCEPT {
    return shz_vec3_scale(vec, shz_vec3_magnitude_inv(vec));
}

#define shz_vec2_deref(ptr) (*((shz_vec2_t*) (ptr)))
#define shz_vec3_deref(ptr) (*((shz_vec3_t*) (ptr)))
#define shz_vec4_deref(ptr) (*((shz_vec4_t*) (ptr)))

SHZ_FORCE_INLINE shz_vec3_t shz_vec2_vec3(shz_vec2_t vec, float z) SHZ_NOEXCEPT {
    return shz_vec3_init(vec.x, vec.y, z);
}

SHZ_FORCE_INLINE shz_vec4_t shz_vec2_vec4(shz_vec2_t vec, float z, float w) SHZ_NOEXCEPT {
    return shz_vec4_init(vec.x, vec.y, z, w);
}

SHZ_FORCE_INLINE shz_vec4_t shz_vec3_vec4(shz_vec3_t vec, float w) SHZ_NOEXCEPT {
    return shz_vec4_init(vec.x, vec.y, vec.z, w);
}

/* ftrv of a vector through XMTRX. The 3- and 2-component forms pass a zero
 * w (and z), exactly as the SH-4 versions build their fv8: they do NOT pick
 * up XMTRX's translation. */
SHZ_FORCE_INLINE shz_vec4_t shz_xmtrx_trans_vec4(shz_vec4_t vec) {
    shz_vec4_t r;
    shz_ftrv_(vec.e, r.e);
    return r;
}

SHZ_FORCE_INLINE shz_vec3_t shz_xmtrx_trans_vec3(shz_vec3_t vec) {
    return shz_xmtrx_trans_vec4(shz_vec4_init(vec.x, vec.y, vec.z, 0.0f)).xyz;
}

SHZ_FORCE_INLINE shz_vec2_t shz_xmtrx_trans_vec2(shz_vec2_t vec) {
    return shz_xmtrx_trans_vec3(shz_vec3_init(vec.x, vec.y, 0.0f)).xy;
}

/* movca.l allocates a cache line without fetching it: an SH-4 cache hint
 * before a full-line overwrite. Nothing to do on the P3. */
SHZ_FORCE_INLINE void shz_dcache_alloc_line(void* src) {
    (void) src;
}

/* ===================================================== XMTRX: loads */

SHZ_INLINE void shz_xmtrx_load_4x4(const shz_matrix_4x4_t* matrix) {
    memcpy(SHZ_X, matrix->elem, sizeof(float) * 16);
}

SHZ_INLINE void shz_xmtrx_load_4x4_unaligned(const float matrix[16]) {
    memcpy(SHZ_X, matrix, sizeof(float) * 16);
}

SHZ_INLINE void shz_xmtrx_load_4x4_transpose(const shz_matrix_4x4_t* matrix) {
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            SHZ_X[4 * c + r] = matrix->elem[4 * r + c];
}

/* row i of XMTRX is r_i: XF[4j + i] = r_i[j] */
SHZ_INLINE void shz_xmtrx_load_4x4_rows(const shz_vec4_t* r1, const shz_vec4_t* r2, const shz_vec4_t* r3,
                                        const shz_vec4_t* r4) {
    const shz_vec4_t* r[4] = { r1, r2, r3, r4 };
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            SHZ_X[4 * j + i] = r[i]->e[j];
}

/* column i of XMTRX is c_i: XF[4i + j] = c_i[j] */
SHZ_INLINE void shz_xmtrx_load_4x4_cols(const shz_vec4_t* c1, const shz_vec4_t* c2, const shz_vec4_t* c3,
                                        const shz_vec4_t* c4) {
    const shz_vec4_t* c[4] = { c1, c2, c3, c4 };
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            SHZ_X[4 * i + j] = c[i]->e[j];
}

/* XMTRX = m1 * m2 */
SHZ_INLINE void shz_xmtrx_load_4x4_apply(const shz_matrix_4x4_t* matrix1, const shz_matrix_4x4_t* matrix2) {
    float m2[16];
    memcpy(m2, matrix2->elem, sizeof(m2));
    memcpy(SHZ_X, matrix1->elem, sizeof(float) * 16);
    shz_xmtrx_mul_(m2);
}

/* out = m1 * m2; XMTRX is left holding m1 (the products go through the
 * other bank, which is not swapped back in) */
SHZ_INLINE void shz_xmtrx_load_4x4_apply_store(shz_matrix_4x4_t* out, const shz_matrix_4x4_t* matrix1,
                                               const shz_matrix_4x4_t* matrix2) {
    float m2[16], r[16];
    memcpy(m2, matrix2->elem, sizeof(m2));
    memcpy(SHZ_X, matrix1->elem, sizeof(float) * 16);
    shz_ftrv_(&m2[0], &r[0]);
    shz_ftrv_(&m2[4], &r[4]);
    shz_ftrv_(&m2[8], &r[8]);
    shz_ftrv_(&m2[12], &r[12]);
    memcpy(out->elem, r, sizeof(r));
}

SHZ_INLINE void shz_xmtrx_load_apply_store_4x4(shz_matrix_4x4_t* out, const shz_matrix_4x4_t* matrix1,
                                               const shz_matrix_4x4_t* matrix2) SHZ_NOEXCEPT {
    shz_xmtrx_load_4x4_apply_store(out, matrix1, matrix2);
}

SHZ_INLINE void shz_xmtrx_load_3x3(const shz_matrix_3x3_t* matrix) {
    const float* m = matrix->elem;
    SHZ_X[0] = m[0]; SHZ_X[1] = m[1]; SHZ_X[2] = m[2];  SHZ_X[3] = 0.0f;
    SHZ_X[4] = m[3]; SHZ_X[5] = m[4]; SHZ_X[6] = m[5];  SHZ_X[7] = 0.0f;
    SHZ_X[8] = m[6]; SHZ_X[9] = m[7]; SHZ_X[10] = m[8]; SHZ_X[11] = 0.0f;
    SHZ_X[12] = 0.0f; SHZ_X[13] = 0.0f; SHZ_X[14] = 0.0f; SHZ_X[15] = 1.0f;
}

SHZ_INLINE void shz_xmtrx_load_3x3_transpose(const float* m) {
    SHZ_X[0] = m[0]; SHZ_X[4] = m[1]; SHZ_X[8] = m[2];
    SHZ_X[1] = m[3]; SHZ_X[5] = m[4]; SHZ_X[9] = m[5];
    SHZ_X[2] = m[6]; SHZ_X[6] = m[7]; SHZ_X[10] = m[8];
    SHZ_X[3] = 0.0f; SHZ_X[7] = 0.0f; SHZ_X[11] = 0.0f;
    SHZ_X[12] = 0.0f; SHZ_X[13] = 0.0f; SHZ_X[14] = 0.0f; SHZ_X[15] = 1.0f;
}

/* ==================================================== XMTRX: stores */

SHZ_INLINE void shz_xmtrx_store_4x4(shz_matrix_4x4_t* matrix) {
    memcpy(matrix->elem, SHZ_X, sizeof(float) * 16);
}

SHZ_INLINE void shz_xmtrx_store_4x4_unaligned(float matrix[16]) {
    memcpy(matrix, SHZ_X, sizeof(float) * 16);
}

SHZ_INLINE void shz_xmtrx_store_4x4_transpose(shz_matrix_4x4_t* matrix) {
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            matrix->elem[4 * r + c] = SHZ_X[4 * c + r];
}

SHZ_INLINE void shz_xmtrx_store_3x3(shz_matrix_3x3_t* matrix) {
    float* m = matrix->elem;
    m[0] = SHZ_X[0]; m[1] = SHZ_X[1]; m[2] = SHZ_X[2];
    m[3] = SHZ_X[4]; m[4] = SHZ_X[5]; m[5] = SHZ_X[6];
    m[6] = SHZ_X[8]; m[7] = SHZ_X[9]; m[8] = SHZ_X[10];
}

SHZ_INLINE void shz_xmtrx_store_3x3_transpose(shz_matrix_3x3_t* matrix) {
    float* m = matrix->elem;
    m[0] = SHZ_X[0]; m[1] = SHZ_X[4]; m[2] = SHZ_X[8];
    m[3] = SHZ_X[1]; m[4] = SHZ_X[5]; m[5] = SHZ_X[9];
    m[6] = SHZ_X[2]; m[7] = SHZ_X[6]; m[8] = SHZ_X[10];
}

SHZ_INLINE void shz_matrix_4x4_copy(shz_matrix_4x4_t* dst, const shz_matrix_4x4_t* src) {
    memmove(dst->elem, src->elem, sizeof(float) * 16);
}

/* ===================================================== XMTRX: apply */

/* XMTRX = XMTRX * [M3 0; 0 1] */
SHZ_INLINE void shz_xmtrx_apply_3x3(const shz_matrix_3x3_t* matrix) {
    const float* m = matrix->elem;
    float t[16] = { m[0], m[1], m[2], 0.0f, m[3], m[4], m[5], 0.0f,
                    m[6], m[7], m[8], 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    shz_xmtrx_mul_(t);
}

SHZ_INLINE void shz_xmtrx_apply_3x3_transpose(const shz_matrix_3x3_t* matrix) {
    const float* m = matrix->elem;
    float t[16] = { m[0], m[3], m[6], 0.0f, m[1], m[4], m[7], 0.0f,
                    m[2], m[5], m[8], 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    shz_xmtrx_mul_(t);
}

SHZ_INLINE void shz_xmtrx_apply_4x4(const shz_matrix_4x4_t* matrix) SHZ_NOEXCEPT {
    float t[16];
    memcpy(t, matrix->elem, sizeof(t));
    shz_xmtrx_mul_(t);
}

SHZ_INLINE void shz_xmtrx_apply_4x4_unaligned(const float matrix[16]) {
    float t[16];
    memcpy(t, matrix, sizeof(t));
    shz_xmtrx_mul_(t);
}

/* ====================================================== XMTRX: init */

SHZ_INLINE void shz_xmtrx_init_identity(void) SHZ_NOEXCEPT {
    memset(SHZ_X, 0, sizeof(float) * 16);
    SHZ_X[0] = SHZ_X[5] = SHZ_X[10] = SHZ_X[15] = 1.0f;
}

SHZ_INLINE void shz_xmtrx_init_identity_safe(void) {
    shz_xmtrx_init_identity();
}

SHZ_INLINE void shz_xmtrx_init_translation(float x, float y, float z) {
    shz_xmtrx_init_identity();
    SHZ_X[12] = x;
    SHZ_X[13] = y;
    SHZ_X[14] = z;
}

SHZ_INLINE void shz_xmtrx_init_diagonal(float x, float y, float z, float w) {
    memset(SHZ_X, 0, sizeof(float) * 16);
    SHZ_X[0] = x;
    SHZ_X[5] = y;
    SHZ_X[10] = z;
    SHZ_X[15] = w;
}

SHZ_FORCE_INLINE void shz_xmtrx_init_scale(float x, float y, float z) {
    shz_xmtrx_init_diagonal(x, y, z, 1.0f);
}

/* The rotation matrices, column-major, as the SH-4 code lays them out:
 *   X: col1 = (0, c, s, 0), col2 = (0, -s, c, 0)
 *   Y: col0 = (c, 0, -s, 0), col2 = (s, 0, c, 0)
 *   Z: col0 = (c, s, 0, 0), col1 = (-s, c, 0, 0) */
SHZ_FORCE_INLINE void shz_rot_x_(float a, float m[16]) {
    shz_sincos_t sc = shz_sincosf(a);
    memset(m, 0, sizeof(float) * 16);
    m[0] = 1.0f;
    m[5] = sc.cos;  m[6] = sc.sin;
    m[9] = -sc.sin; m[10] = sc.cos;
    m[15] = 1.0f;
}

SHZ_FORCE_INLINE void shz_rot_y_(float a, float m[16]) {
    shz_sincos_t sc = shz_sincosf(a);
    memset(m, 0, sizeof(float) * 16);
    m[0] = sc.cos;  m[2] = -sc.sin;
    m[5] = 1.0f;
    m[8] = sc.sin;  m[10] = sc.cos;
    m[15] = 1.0f;
}

SHZ_FORCE_INLINE void shz_rot_z_(float a, float m[16]) {
    shz_sincos_t sc = shz_sincosf(a);
    memset(m, 0, sizeof(float) * 16);
    m[0] = sc.cos;  m[1] = sc.sin;
    m[4] = -sc.sin; m[5] = sc.cos;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

SHZ_INLINE void shz_xmtrx_init_rotation_x(float x) {
    shz_rot_x_(x, SHZ_X);
}

SHZ_INLINE void shz_xmtrx_init_rotation_y(float y) {
    shz_rot_y_(y, SHZ_X);
}

SHZ_INLINE void shz_xmtrx_init_rotation_z(float z) {
    shz_rot_z_(z, SHZ_X);
}

SHZ_INLINE void shz_xmtrx_rotate_x(float x) {
    float m[16];
    shz_rot_x_(x, m);
    shz_xmtrx_mul_(m);
}

SHZ_INLINE void shz_xmtrx_rotate_y(float y) {
    float m[16];
    shz_rot_y_(y, m);
    shz_xmtrx_mul_(m);
}

SHZ_INLINE void shz_xmtrx_rotate_z(float z) {
    float m[16];
    shz_rot_z_(z, m);
    shz_xmtrx_mul_(m);
}

/* The apply_rotation forms replace only the two columns the rotation
 * touches, with those columns run through XMTRX: the same product. */
SHZ_INLINE void shz_xmtrx_apply_rotation_x(float x) {
    shz_xmtrx_rotate_x(x);
}

SHZ_INLINE void shz_xmtrx_apply_rotation_y(float y) {
    shz_xmtrx_rotate_y(y);
}

SHZ_INLINE void shz_xmtrx_apply_rotation_z(float z) {
    shz_xmtrx_rotate_z(z);
}

SHZ_FORCE_INLINE void shz_rot_axis_(float angle, float x, float y, float z, float m[16]) {
    shz_sincos_t sc = shz_sincosf(angle);
    const float s = sc.sin, c = sc.cos, ic = 1.0f - c;
    const float inv = shz_inv_sqrtf(x * x + y * y + z * z);

    x *= inv;
    y *= inv;
    z *= inv;
    m[0] = x * x * ic + c;     m[1] = x * y * ic + z * s; m[2] = x * z * ic - y * s;  m[3] = 0.0f;
    m[4] = x * y * ic - z * s; m[5] = y * y * ic + c;     m[6] = y * z * ic + x * s;  m[7] = 0.0f;
    m[8] = x * z * ic + y * s; m[9] = y * z * ic - x * s; m[10] = z * z * ic + c;     m[11] = 0.0f;
    m[12] = 0.0f;              m[13] = 0.0f;              m[14] = 0.0f;               m[15] = 1.0f;
}

SHZ_INLINE void shz_xmtrx_apply_rotation_axis(float angle, float x, float y, float z) {
    float m[16];
    shz_rot_axis_(angle, x, y, z, m);
    shz_xmtrx_mul_(m);
}

SHZ_INLINE void shz_xmtrx_init_rotation_axis(float angle, float x, float y, float z) {
    shz_rot_axis_(angle, x, y, z, SHZ_X);
}

SHZ_INLINE void shz_xmtrx_init_rotation(float roll, float pitch, float yaw) {
    shz_xmtrx_init_rotation_z(yaw);
    shz_xmtrx_apply_rotation_y(pitch);
    shz_xmtrx_apply_rotation_x(roll);
}

SHZ_INLINE void shz_xmtrx_apply_rotation(float roll, float pitch, float yaw) {
    shz_xmtrx_apply_rotation_x(roll);
    shz_xmtrx_apply_rotation_y(pitch);
    shz_xmtrx_apply_rotation_z(yaw);
}

SHZ_FORCE_INLINE void shz_xmtrx_set_translation(float x, float y, float z) {
    SHZ_X[12] = x;
    SHZ_X[13] = y;
    SHZ_X[14] = z;
}

/* As the SH-4 sequence: the offset is ADDED to the translation column, not
 * multiplied through. */
SHZ_INLINE void shz_xmtrx_apply_translation(float x, float y, float z) {
    SHZ_X[12] += x;
    SHZ_X[13] += y;
    SHZ_X[14] += z;
}

/* XMTRX = XMTRX * T */
SHZ_INLINE void shz_xmtrx_translate(float x, float y, float z) {
    float m[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f, x, y, z, 1.0f };
    shz_xmtrx_mul_(m);
}

/* As the SH-4 sequence: the three upper rows of the first three columns are
 * scaled; row 3 and the translation column are left alone. */
SHZ_INLINE void shz_xmtrx_apply_scale(float x, float y, float z) {
    SHZ_X[0] *= x; SHZ_X[1] *= x; SHZ_X[2] *= x;
    SHZ_X[4] *= y; SHZ_X[5] *= y; SHZ_X[6] *= y;
    SHZ_X[8] *= z; SHZ_X[9] *= z; SHZ_X[10] *= z;
}

/* XMTRX = XMTRX * S */
SHZ_INLINE void shz_xmtrx_scale(float x, float y, float z) {
    float m[16] = { x, 0.0f, 0.0f, 0.0f, 0.0f, y, 0.0f, 0.0f,
                    0.0f, 0.0f, z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    shz_xmtrx_mul_(m);
}

SHZ_INLINE void shz_xmtrx_transpose(void) {
    float t;
    for (int c = 0; c < 4; c++)
        for (int r = c + 1; r < 4; r++) {
            t = SHZ_X[4 * c + r];
            SHZ_X[4 * c + r] = SHZ_X[4 * r + c];
            SHZ_X[4 * r + c] = t;
        }
}

/* ======================================= matrices in memory, by value */

/* (M * (v, 0)).xyz */
SHZ_INLINE shz_vec3_t shz_mat4x4_trans_vec3(const shz_matrix_4x4_t* m, shz_vec3_t v) SHZ_NOEXCEPT {
    shz_vec3_t out;
    out.x = v.x * m->elem2D[0][0] + v.y * m->elem2D[1][0] + v.z * m->elem2D[2][0];
    out.y = v.x * m->elem2D[0][1] + v.y * m->elem2D[1][1] + v.z * m->elem2D[2][1];
    out.z = v.x * m->elem2D[0][2] + v.y * m->elem2D[1][2] + v.z * m->elem2D[2][2];
    return out;
}

SHZ_INLINE shz_vec4_t shz_mat4x4_trans_vec4(const shz_matrix_4x4_t* mat, shz_vec4_t in) SHZ_NOEXCEPT {
    shz_vec4_t out;
    for (int r = 0; r < 4; r++)
        out.e[r] = mat->col[0].e[r] * in.x + mat->col[1].e[r] * in.y + mat->col[2].e[r] * in.z +
                   mat->col[3].e[r] * in.w;
    return out;
}

SHZ_INLINE shz_vec3_t shz_matrix3x3_trans_vec3(const shz_matrix_3x3_t* m, shz_vec3_t v) {
    shz_vec3_t out;
    out.x = v.x * m->elem2D[0][0] + v.y * m->elem2D[1][0] + v.z * m->elem2D[2][0];
    out.y = v.x * m->elem2D[0][1] + v.y * m->elem2D[1][1] + v.z * m->elem2D[2][1];
    out.z = v.x * m->elem2D[0][2] + v.y * m->elem2D[1][2] + v.z * m->elem2D[2][2];
    return out;
}

/* (M^T * (v, 0)).xyz */
SHZ_INLINE shz_vec3_t shz_matrix4x4_trans_vec3_transpose(const shz_matrix_4x4_t* m, shz_vec3_t v) {
    shz_vec3_t out;
    out.x = v.x * m->elem2D[0][0] + v.y * m->elem2D[0][1] + v.z * m->elem2D[0][2];
    out.y = v.x * m->elem2D[1][0] + v.y * m->elem2D[1][1] + v.z * m->elem2D[1][2];
    out.z = v.x * m->elem2D[2][0] + v.y * m->elem2D[2][1] + v.z * m->elem2D[2][2];
    return out;
}

SHZ_INLINE shz_vec3_t shz_matrix3x3_trans_vec3_transpose(const shz_matrix_3x3_t* m, shz_vec3_t v) {
    shz_vec3_t out;
    out.x = v.x * m->elem2D[0][0] + v.y * m->elem2D[0][1] + v.z * m->elem2D[0][2];
    out.y = v.x * m->elem2D[1][0] + v.y * m->elem2D[1][1] + v.z * m->elem2D[1][2];
    out.z = v.x * m->elem2D[2][0] + v.y * m->elem2D[2][1] + v.z * m->elem2D[2][2];
    return out;
}

/* ============================================================= memory */

SHZ_INLINE void shz_memcpy4_16(void* dst, const void* src) {
    memcpy(dst, src, 64);
}

SHZ_FORCE_INLINE void* shz_memcpy1(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    return memcpy(dst, src, bytes);
}

SHZ_INLINE void* shz_memcpy2(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    return memcpy(dst, src, bytes & ~(size_t) 1);
}

SHZ_INLINE void* shz_memcpy4(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    return memcpy(dst, src, bytes & ~(size_t) 3);
}

SHZ_INLINE void* shz_memcpy8(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    return memcpy(dst, src, bytes & ~(size_t) 7);
}

SHZ_INLINE void* shz_memcpy32(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    return memcpy(dst, src, bytes & ~(size_t) 31);
}

SHZ_INLINE void* shz_sq_memcpy32(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    return memcpy(dst, src, bytes & ~(size_t) 31);
}

SHZ_INLINE void* shz_memcpy64(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    return memcpy(dst, src, bytes & ~(size_t) 63);
}

SHZ_INLINE void* shz_memcpy128(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    return memcpy(dst, src, bytes & ~(size_t) 127);
}

SHZ_INLINE void* shz_memcpy(void* SHZ_RESTRICT dst, const void* SHZ_RESTRICT src, size_t bytes) SHZ_NOEXCEPT {
    return memcpy(dst, src, bytes);
}

#ifdef __cplusplus
}
#endif

#endif /* SH4ZAM_X86_H */
