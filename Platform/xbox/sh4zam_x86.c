/* sh4zam_x86.c -- the one definition of the emulated XMTRX register bank
 * (see sh4zam_x86.h). Identity at boot, as the SH-4's is after KOS's FPU
 * setup leaves it undefined: nothing in the game reads XMTRX before loading
 * it, so any defined value would do; identity is the harmless one. */
#include "sh4zam_x86.h"

float shz_xmtrx_[16] __attribute__((aligned(16))) = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
};

/* The port's sqrtf, in place of the SDK libc's: picolibc's sf_sqrt.c works
 * the root out a bit at a time in software (and asks fegetround on most
 * calls), where the Pentium III's sqrtss is one instruction with the same
 * correctly rounded result. The game's collision, camera and homing code
 * and the audio take square roots throughout; the linker takes this
 * definition and never pulls libc's (its object defines nothing else). */
float sqrtf(float x) {
    return shz_sqrtss_(x);
}
