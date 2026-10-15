/* pvr_texdecode.h -- PowerVR texture memory to linear texels.
 *
 * Pure C with no Xbox headers, so the renderer (pvr_xbox.c) and the host
 * test (tools/xbox/hosttest/test_decode.c, which holds it to the port's
 * software PVR, tools/lib/pvrsoft.py) compile the same code.
 *
 * The layouts are the PVR's, as KOS's loader writes them and pvrsoft.py
 * reads them:
 *   twiddled   texel (x, y) of a w x h texture is number
 *              spread(x mod m) << 1 | spread(y mod m), m = min(w, h), plus
 *              the square's index << 2*log2(m) along the longer side --
 *              y in the even bits, x in the odd (KOS's TWIDOUT)
 *   4bpp       two texels a byte, the lower-numbered in the low nibble
 *   VQ         a 2048-byte codebook of 256 2x2 blocks (x0y0, x0y1, x1y0,
 *              x1y1), then one byte per block, the blocks twiddled
 *   stride     non-twiddled rows of `stride` pixels (X32_STRIDE)
 */
#ifndef XBOX_PVR_TEXDECODE_H
#define XBOX_PVR_TEXDECODE_H

#include <stdint.h>
#include <string.h>

#define PVR_TD_ARGB1555 0
#define PVR_TD_RGB565   1
#define PVR_TD_ARGB4444 2
#define PVR_TD_PAL4     5
#define PVR_TD_PAL8     6

/* KOS's TWIDTAB, for ten bits */
static inline uint32_t pvr_td_spread(uint32_t v)
{
    v &= 0x3ff;
    return (v & 1) | ((v & 2) << 1) | ((v & 4) << 2) | ((v & 8) << 3) | ((v & 16) << 4) |
           ((v & 32) << 5) | ((v & 64) << 6) | ((v & 128) << 7) | ((v & 256) << 8) |
           ((v & 512) << 9);
}

static inline uint32_t pvr_td_twiddle(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                      uint32_t min, uint32_t nb)
{
    uint32_t idx = (pvr_td_spread(x & (min - 1)) << 1) | pvr_td_spread(y & (min - 1));

    if (w > h)
        idx |= (x >> nb) << (2 * nb);
    else if (h > w)
        idx |= (y >> nb) << (2 * nb);
    return idx;
}

static inline uint32_t pvr_td_log2(uint32_t v)
{
    uint32_t n = 0;

    while ((1u << n) < v)
        n++;
    return n;
}

/* The bytes a texture of this header occupies in texture memory. */
static inline uint32_t pvr_td_bytes(uint32_t w, uint32_t h, uint32_t m3)
{
    int pixfmt = (int)((m3 >> 27) & 7);
    uint32_t b;

    if ((m3 >> 30) & 1)
        return 2048 + w * h / 4;
    switch (pixfmt)
    {
        case PVR_TD_PAL4: b = w * h / 2; break;
        case PVR_TD_PAL8: b = w * h; break;
        default: b = w * h * 2; break;
    }
    if ((m3 >> 31) & 1)
        b = b * 4 / 3 + 8;
    return b;
}

/* Byte offset of the largest level of a twiddled mipmap chain: the smaller
 * levels come first from 1x1 up, after a pad of 6 bytes (16-bit), 3 (8-bit)
 * or 1 (4-bit). */
static inline uint32_t pvr_td_mip_base(uint32_t w, int pixfmt)
{
    uint32_t off, s;

    off = (pixfmt == PVR_TD_PAL4) ? 1 : (pixfmt == PVR_TD_PAL8) ? 3 : 6;
    for (s = 1; s < w; s <<= 1)
    {
        uint32_t texels = s * s;

        off += (pixfmt == PVR_TD_PAL4) ? texels / 2 : (pixfmt == PVR_TD_PAL8) ? texels : texels * 2;
    }
    return off;
}

/* Decode the w x h texture mode3 `m3` describes, at `src`, into `lin`:
 * row-major, 2 bytes a texel (16-bit formats, the PVR's own bits) or 1 (a
 * palette index for the 4 and 8 bpp formats). `stride` is the stride
 * register's width in pixels for an X32_STRIDE texture; `avail` the bytes
 * of texture memory readable from src. Returns the bytes per texel, or 0
 * for a texture this cannot decode (YUV, bump, VQ paletted or mipmapped). */
static inline int pvr_td_decode(const uint8_t *src, uint32_t w, uint32_t h, uint32_t m3,
                                uint32_t stride, uint32_t avail, uint8_t *lin)
{
    int pixfmt = (int)((m3 >> 27) & 7);
    int vq = (int)((m3 >> 30) & 1);
    int nontwid = (int)((m3 >> 26) & 1);
    int xstride = (int)((m3 >> 25) & 1);
    int mip = (int)((m3 >> 31) & 1);
    uint32_t min = w < h ? w : h, nb = pvr_td_log2(min), x, y;
    int bpp;

    switch (pixfmt)
    {
        case PVR_TD_ARGB1555:
        case PVR_TD_RGB565:
        case PVR_TD_ARGB4444:
            bpp = 2;
            break;
        case PVR_TD_PAL4:
        case PVR_TD_PAL8:
            /* A paletted texture is always twiddled: bits 21-26 (4 bpp) or
             * 25-26 (8 bpp) of mode3 are its palette bank, so the
             * non-twiddled and stride bits are not what they are for a
             * 16-bit texture. Reading bit 26 as "not twiddled" scrambles
             * every 4 bpp texture in banks 32-63. */
            bpp = 1;
            nontwid = 0;
            xstride = 0;
            break;
        default:
            return 0;
    }
    if (vq && (bpp == 1 || mip))
        return 0;
    if (!nontwid && pvr_td_bytes(w, h, m3) > avail)
        return 0;
    if (mip && !vq && !nontwid)
        src += pvr_td_mip_base(w, pixfmt);

    if (vq)
    {
        const uint16_t *book = (const uint16_t *)src;
        const uint8_t *idx = src + 2048;
        uint32_t hw = w / 2, hh = h / 2, hmin = hw < hh ? hw : hh, hnb = pvr_td_log2(hmin);
        uint16_t *out = (uint16_t *)lin;

        for (y = 0; y < hh; y++)
            for (x = 0; x < hw; x++)
            {
                const uint16_t *k = &book[idx[pvr_td_twiddle(x, y, hw, hh, hmin, hnb)] * 4];

                out[(2 * y) * w + 2 * x] = k[0];
                out[(2 * y + 1) * w + 2 * x] = k[1];
                out[(2 * y) * w + 2 * x + 1] = k[2];
                out[(2 * y + 1) * w + 2 * x + 1] = k[3];
            }
    }
    else if (pixfmt == PVR_TD_PAL4)
    {
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
            {
                uint32_t t = nontwid ? y * w + x : pvr_td_twiddle(x, y, w, h, min, nb);

                lin[y * w + x] = (src[t >> 1] >> ((t & 1) * 4)) & 15;
            }
    }
    else if (pixfmt == PVR_TD_PAL8)
    {
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                lin[y * w + x] = src[nontwid ? y * w + x : pvr_td_twiddle(x, y, w, h, min, nb)];
    }
    else if (nontwid)
    {
        uint32_t pitch = (xstride && stride) ? stride : w;
        uint32_t rows = h, cols = w < pitch ? w : pitch;

        if (pitch * rows * 2 > avail)
            rows = avail / (pitch * 2);
        memset(lin, 0, (size_t)w * h * 2);
        for (y = 0; y < rows; y++)
            memcpy(lin + (size_t)y * w * 2, src + (size_t)y * pitch * 2, cols * 2);
    }
    else
    {
        const uint16_t *s16 = (const uint16_t *)src;
        uint16_t *o16 = (uint16_t *)lin;

        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                o16[y * w + x] = s16[pvr_td_twiddle(x, y, w, h, min, nb)];
    }
    return bpp;
}

#endif /* XBOX_PVR_TEXDECODE_H */
