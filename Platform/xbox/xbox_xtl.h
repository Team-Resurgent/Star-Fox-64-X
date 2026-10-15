/* xbox_xtl.h -- <xtl.h> for the Platform/xbox C files.
 *
 * d3dx8math.inl defines its helpers `inline` without `static`; in C that
 * makes every translation unit including <xtl.h> emit the same external
 * symbols, and the second such file fails the link (the MK64-X lesson that
 * once allowed only one C file to include it). Defining the .inl's own
 * include guard first skips it: no C file here uses the D3DX math
 * helpers. The C++ renderer is unaffected (C++ inline is COMDAT). */
#ifndef XBOX_XTL_WRAPPER_H
#define XBOX_XTL_WRAPPER_H

#ifndef __cplusplus
#define __D3DX8MATH_INL__
#endif
#include <xtl.h>

#endif /* XBOX_XTL_WRAPPER_H */
