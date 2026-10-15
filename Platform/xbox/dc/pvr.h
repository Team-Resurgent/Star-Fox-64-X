/* dc/pvr.h -- Xbox shim: KallistiOS's PowerVR API, implemented over Direct3D 8
 * on the NV2A (Platform/xbox/pvr_xbox.cpp).
 *
 * Types, constants and header bit layouts are KallistiOS's own (c22f26c,
 * dc/pvr.h and dc/pvr/*.h, BSD-style KOS licence), so the port compiles
 * its polygon headers exactly as it does on the Dreamcast and the Xbox
 * backend decodes the same 32-byte words the TA would have received.
 *
 * What the backend models (see pvr_xbox.cpp for the mapping):
 *   - texture RAM: an 8 MB host buffer aligned to 16 MB, so a pvr_ptr_t is
 *     an ordinary pointer the game may write through, and both KOS's
 *     `(addr & (PVR_RAM_SIZE - 1)) >> 3` header encoding and the port's
 *     `(addr & 0x00fffff8) >> 3` (src/gfx/gfx_pvr.c) still name it;
 *   - palette RAM: 1024 entries, written by pvr_set_pal_entry;
 *   - the three lists the port uses (OP, PT, TR), PT's alpha reference, the
 *     TR list's per-pixel autosort, render-to-texture, the background
 *     colour, and the Z clip plane. */
#ifndef XBOX_DC_PVR_H
#define XBOX_DC_PVR_H

#include <kos/cdefs.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <dc/sq.h>

__BEGIN_DECLS

/* ================================================================ memory */

typedef void *pvr_ptr_t;

#define PVR_RAM_SIZE_MB 8
#define PVR_RAM_SIZE    (PVR_RAM_SIZE_MB * 1024 * 1024)

pvr_ptr_t pvr_mem_malloc(size_t size);
void pvr_mem_free(pvr_ptr_t chunk);
size_t pvr_mem_available(void);
void pvr_mem_reset(void);

/* ======================================================= header layout */

typedef enum pvr_color_fmts {
    PVR_CLRFMT_ARGBPACKED,
    PVR_CLRFMT_4FLOATS,
    PVR_CLRFMT_INTENSITY,
    PVR_CLRFMT_INTENSITY_PREV,
} pvr_color_fmts_t;

typedef enum pvr_clip_mode {
    PVR_USERCLIP_DISABLE = 0,
    PVR_USERCLIP_INSIDE = 2,
    PVR_USERCLIP_OUTSIDE = 3,
} pvr_clip_mode_t;

typedef enum pvr_list_type {
    PVR_LIST_OP_POLY,
    PVR_LIST_OP_MOD,
    PVR_LIST_TR_POLY,
    PVR_LIST_TR_MOD,
    PVR_LIST_PT_POLY,
    PVR_LIST_NONE = -1
} pvr_list_t;

#define pvr_list_type_t pvr_list_t

typedef enum pvr_cull_mode {
    PVR_CULLING_NONE,
    PVR_CULLING_SMALL,
    PVR_CULLING_CCW,
    PVR_CULLING_CW,
} pvr_cull_mode_t;

typedef enum pvr_depthcmp_mode {
    PVR_DEPTHCMP_NEVER,
    PVR_DEPTHCMP_LESS,
    PVR_DEPTHCMP_EQUAL,
    PVR_DEPTHCMP_LEQUAL,
    PVR_DEPTHCMP_GREATER,
    PVR_DEPTHCMP_NOTEQUAL,
    PVR_DEPTHCMP_GEQUAL,
    PVR_DEPTHCMP_ALWAYS,
} pvr_depthcmp_mode_t;

typedef enum pvr_uv_size {
    PVR_UV_SIZE_8,
    PVR_UV_SIZE_16,
    PVR_UV_SIZE_32,
    PVR_UV_SIZE_64,
    PVR_UV_SIZE_128,
    PVR_UV_SIZE_256,
    PVR_UV_SIZE_512,
    PVR_UV_SIZE_1024,
} pvr_uv_size_t;

typedef enum pvr_txr_shading_mode {
    PVR_TXRENV_REPLACE,
    PVR_TXRENV_MODULATE,
    PVR_TXRENV_DECAL,
    PVR_TXRENV_MODULATEALPHA,
} pvr_txr_shading_mode_t;

typedef enum pvr_filter_mode {
    PVR_FILTER_NEAREST,
    PVR_FILTER_BILINEAR,
    PVR_FILTER_TRILINEAR1,
    PVR_FILTER_TRILINEAR2,
    PVR_FILTER_NONE = PVR_FILTER_NEAREST,
} pvr_filter_mode_t;

typedef enum pvr_fog_type {
    PVR_FOG_TABLE,
    PVR_FOG_VERTEX,
    PVR_FOG_DISABLE,
    PVR_FOG_TABLE2,
} pvr_fog_type_t;

typedef enum pvr_blend_mode {
    PVR_BLEND_ZERO,
    PVR_BLEND_ONE,
    PVR_BLEND_DESTCOLOR,
    PVR_BLEND_INVDESTCOLOR,
    PVR_BLEND_SRCALPHA,
    PVR_BLEND_INVSRCALPHA,
    PVR_BLEND_DESTALPHA,
    PVR_BLEND_INVDESTALPHA,
} pvr_blend_mode_t;

typedef enum pvr_pixel_mode {
    PVR_PIXEL_MODE_ARGB1555,
    PVR_PIXEL_MODE_RGB565,
    PVR_PIXEL_MODE_ARGB4444,
    PVR_PIXEL_MODE_YUV422,
    PVR_PIXEL_MODE_BUMP,
    PVR_PIXEL_MODE_PAL_4BPP,
    PVR_PIXEL_MODE_PAL_8BPP,
} pvr_pixel_mode_t;

typedef enum pvr_strip_len {
    PVR_STRIP_LEN_1,
    PVR_STRIP_LEN_2,
    PVR_STRIP_LEN_4,
    PVR_STRIP_LEN_6,
} pvr_strip_len_t;

typedef enum pvr_hdr_type {
    PVR_HDR_EOL = 0,
    PVR_HDR_USERCLIP = 1,
    PVR_HDR_OBJECT_LIST_SET = 2,
    PVR_HDR_POLY = 4,
    PVR_HDR_SPRITE = 5,
} pvr_hdr_type_t;

typedef uint32_t pvr_txr_ptr_t;

static inline pvr_txr_ptr_t to_pvr_txr_ptr(pvr_ptr_t addr) {
    return ((uint32_t)(uintptr_t)addr & (PVR_RAM_SIZE - 1)) >> 3;
}
#define pvr_to_pvr_txr_ptr(addr) to_pvr_txr_ptr(addr)

typedef struct pvr_poly_hdr_cmd {
    bool uvfmt_f16               :1;
    bool gouraud                 :1;
    bool oargb_en                :1;
    bool txr_en                  :1;
    pvr_color_fmts_t color_fmt   :2;
    bool mod_normal              :1;
    bool modifier_en             :1;
    uint32_t                     :8;
    pvr_clip_mode_t clip_mode    :2;
    pvr_strip_len_t strip_len    :2;
    uint32_t                     :3;
    bool auto_strip_len          :1;
    pvr_list_t list_type         :3;
    uint32_t                     :1;
    bool strip_end               :1;
    pvr_hdr_type_t hdr_type      :3;
} pvr_poly_hdr_cmd_t;

typedef struct pvr_poly_hdr_mode1 {
    uint32_t                      :25;
    bool txr_en                   :1;
    bool depth_write_dis          :1;
    pvr_cull_mode_t culling       :2;
    pvr_depthcmp_mode_t depth_cmp :3;
} pvr_poly_hdr_mode1_t;

typedef struct pvr_poly_hdr_mode2 {
    pvr_uv_size_t v_size           :3;
    pvr_uv_size_t u_size           :3;
    pvr_txr_shading_mode_t shading :2;
    uint32_t mip_bias              :4;
    bool supersampling             :1;
    pvr_filter_mode_t filter_mode  :2;
    bool v_clamp                   :1;
    bool u_clamp                   :1;
    bool v_flip                    :1;
    bool u_flip                    :1;
    bool txralpha_dis              :1;
    bool alpha                     :1;
    bool fog_clamp                 :1;
    pvr_fog_type_t fog_type        :2;
    bool blend_dst_acc2            :1;
    bool blend_src_acc2            :1;
    pvr_blend_mode_t blend_dst     :3;
    pvr_blend_mode_t blend_src     :3;
} pvr_poly_hdr_mode2_t;

typedef struct pvr_poly_hdr_mode3 {
    pvr_txr_ptr_t txr_base       :25;
    bool x32stride               :1;
    bool nontwiddled             :1;
    pvr_pixel_mode_t pixel_mode  :3;
    bool vq_en                   :1;
    bool mipmap_en               :1;
} pvr_poly_hdr_mode3_t;

_Static_assert(sizeof(pvr_poly_hdr_cmd_t) == 4, "cmd layout");
_Static_assert(sizeof(pvr_poly_hdr_mode1_t) == 4, "mode1 layout");
_Static_assert(sizeof(pvr_poly_hdr_mode2_t) == 4, "mode2 layout");
_Static_assert(sizeof(pvr_poly_hdr_mode3_t) == 4, "mode3 layout");

typedef struct __attribute__((aligned(32))) pvr_poly_hdr {
    union {
        uint32_t cmd;
        pvr_poly_hdr_cmd_t m0;
    };
    union {
        uint32_t mode1;
        pvr_poly_hdr_mode1_t m1;
    };
    union {
        uint32_t mode2;
        uint32_t mode2_0;
        pvr_poly_hdr_mode2_t m2;
    };
    union {
        uint32_t mode3;
        uint32_t mode3_0;
        pvr_poly_hdr_mode3_t m3;
    };
    union {
        struct {
            float a;
            float r;
            float g;
            float b;
        };
        struct {
            union {
                struct {
                    uint32_t mode2_1;
                    uint32_t mode3_1;
                };
                struct {
                    pvr_poly_hdr_mode2_t m2;
                    pvr_poly_hdr_mode3_t m3;
                } modifier;
            };
        };
        struct {
            uint32_t argb;
            uint32_t oargb;
        };
        struct {
            uint32_t start_x;
            uint32_t start_y;
            uint32_t end_x;
            uint32_t end_y;
        };
    };
} pvr_poly_hdr_t;

_Static_assert(sizeof(pvr_poly_hdr_t) == 32, "Invalid header size");

/* ====================================================== polygon context */

typedef enum pvr_mip_bias {
    PVR_MIPBIAS_0_25   = 1,
    PVR_MIPBIAS_0_50   = 2,
    PVR_MIPBIAS_0_75   = 3,
    PVR_MIPBIAS_1_00   = 4,
    PVR_MIPBIAS_1_25   = 5,
    PVR_MIPBIAS_1_50   = 6,
    PVR_MIPBIAS_1_75   = 7,
    PVR_MIPBIAS_2_00   = 8,
    PVR_MIPBIAS_2_25   = 9,
    PVR_MIPBIAS_2_50   = 10,
    PVR_MIPBIAS_2_75   = 11,
    PVR_MIPBIAS_3_00   = 12,
    PVR_MIPBIAS_3_25   = 13,
    PVR_MIPBIAS_3_50   = 14,
    PVR_MIPBIAS_3_75   = 15,
    PVR_MIPBIAS_NORMAL = PVR_MIPBIAS_1_00
} pvr_mip_bias_t;

typedef enum pvr_uv_flip {
    PVR_UVFLIP_NONE,
    PVR_UVFLIP_V,
    PVR_UVFLIP_U,
    PVR_UVFLIP_UV
} pvr_uv_flip_t;

typedef enum pvr_uv_clamp {
    PVR_UVCLAMP_NONE,
    PVR_UVCLAMP_V,
    PVR_UVCLAMP_U,
    PVR_UVCLAMP_UV
} pvr_uv_clamp_t;

typedef struct {
    pvr_list_t  list_type;
    struct {
        bool                alpha;
        bool                shading;
        pvr_fog_type_t      fog_type;
        pvr_cull_mode_t     culling;
        bool                color_clamp;
        pvr_clip_mode_t     clip_mode;
        bool                modifier_mode;
        bool                specular;
        bool                alpha2;
        pvr_fog_type_t      fog_type2;
        bool                color_clamp2;
    } gen;
    struct {
        pvr_blend_mode_t    src;
        pvr_blend_mode_t    dst;
        bool                src_enable;
        bool                dst_enable;
        pvr_blend_mode_t    src2;
        pvr_blend_mode_t    dst2;
        bool                src_enable2;
        bool                dst_enable2;
    } blend;
    struct {
        pvr_color_fmts_t    color;
        bool                uv;
        bool                modifier;
    } fmt;
    struct {
        pvr_depthcmp_mode_t comparison;
        bool                write;
    } depth;
    struct {
        bool                enable;
        pvr_filter_mode_t   filter;
        bool                mipmap;
        pvr_mip_bias_t      mipmap_bias;
        pvr_uv_flip_t       uv_flip;
        pvr_uv_clamp_t      uv_clamp;
        bool                alpha;
        pvr_txr_shading_mode_t  env;
        int     width;
        int     height;
        int     format;
        pvr_ptr_t base;
    } txr, txr2;
} pvr_poly_cxt_t;

/* Legacy constant names (KOS dc/pvr/pvr_legacy.h), with KOS's values.
 * NOTE the two inverted pairs: mode1 bit 26 DISABLES depth writes and
 * mode2 bit 19 DISABLES texture alpha, so "ENABLE" is 0 for both. */
#define PVR_ALPHA_DISABLE       0
#define PVR_ALPHA_ENABLE        1
#define PVR_SHADE_FLAT          0
#define PVR_SHADE_GOURAUD       1
#define PVR_CLRCLAMP_DISABLE    0
#define PVR_CLRCLAMP_ENABLE     1
#define PVR_SPECULAR_DISABLE    0
#define PVR_SPECULAR_ENABLE     1
#define PVR_MODIFIER_CHEAP_SHADOW   0
#define PVR_MODIFIER_NORMAL         1
#define PVR_BLEND_DISABLE       0
#define PVR_BLEND_ENABLE        1
#define PVR_UVFMT_32BIT         0
#define PVR_UVFMT_16BIT         1
#define PVR_MODIFIER_DISABLE    0
#define PVR_MODIFIER_ENABLE     1
#define PVR_DEPTHWRITE_ENABLE   0
#define PVR_DEPTHWRITE_DISABLE  1
#define PVR_TEXTURE_DISABLE     0
#define PVR_TEXTURE_ENABLE      1
#define PVR_MIPMAP_DISABLE      0
#define PVR_MIPMAP_ENABLE       1
#define PVR_TXRALPHA_ENABLE     0
#define PVR_TXRALPHA_DISABLE    1

/* =================================================== texture formats */

#define PVR_TXRFMT_NONE         0
#define PVR_TXRFMT_VQ_DISABLE   (0 << 30)
#define PVR_TXRFMT_VQ_ENABLE    (1 << 30)
#define PVR_TXRFMT_ARGB1555     (0 << 27)
#define PVR_TXRFMT_RGB565       (1 << 27)
#define PVR_TXRFMT_ARGB4444     (2 << 27)
#define PVR_TXRFMT_YUV422       (3 << 27)
#define PVR_TXRFMT_BUMP         (4 << 27)
#define PVR_TXRFMT_PAL4BPP      (5 << 27)
#define PVR_TXRFMT_PAL8BPP      (6 << 27)
#define PVR_TXRFMT_TWIDDLED     (0 << 26)
#define PVR_TXRFMT_NONTWIDDLED  (1 << 26)
#define PVR_TXRFMT_POW2_STRIDE  (0 << 25)
#define PVR_TXRFMT_X32_STRIDE   (1 << 25)
#define PVR_TXRFMT_8BPP_PAL(x)  ((x) << 25)
#define PVR_TXRFMT_4BPP_PAL(x)  ((x) << 21)

/* ============================================================ vertices */

typedef struct pvr_vertex {
    alignas(32)
    uint32_t flags;
    float   x;
    float   y;
    float   z;
    union {
        struct {
            float u;
            float v;
        };
        struct {
            uint32_t argb0;
            uint32_t argb1;
        };
    };
    uint32_t argb;
    uint32_t oargb;
} pvr_vertex_t;

_Static_assert(sizeof(pvr_vertex_t) == 32, "pvr_vertex_t layout");

#define PVR_CMD_POLYHDR     0x80840000
#define PVR_CMD_VERTEX      0xe0000000
#define PVR_CMD_VERTEX_EOL  0xf0000000
#define PVR_CMD_USERCLIP    0x20000000
#define PVR_CMD_MODIFIER    0x80000000
#define PVR_CMD_SPRITE      0xA0000000

#define PVR_TA_CMD_TYPE            GENMASK(26, 24)
#define PVR_TA_CMD_USERCLIP        GENMASK(17, 16)
#define PVR_TA_CMD_MODIFIER        BIT(7)
#define PVR_TA_CMD_MODIFIERMODE    BIT(6)
#define PVR_TA_CMD_CLRFMT          GENMASK(5, 4)
#define PVR_TA_CMD_TXRENABLE       BIT(3)
#define PVR_TA_CMD_SPECULAR        BIT(2)
#define PVR_TA_CMD_SHADE           BIT(1)
#define PVR_TA_CMD_UVFMT           BIT(0)
#define PVR_TA_PM1_DEPTHCMP        GENMASK(31, 29)
#define PVR_TA_PM1_CULLING         GENMASK(28, 27)
#define PVR_TA_PM1_DEPTHWRITE      BIT(26)
#define PVR_TA_PM1_TXRENABLE       BIT(25)
#define PVR_TA_PM1_MODIFIERINST    GENMASK(30, 29)
#define PVR_TA_PM2_SRCBLEND        GENMASK(31, 29)
#define PVR_TA_PM2_DSTBLEND        GENMASK(28, 26)
#define PVR_TA_PM2_SRCENABLE       BIT(25)
#define PVR_TA_PM2_DSTENABLE       BIT(24)
#define PVR_TA_PM2_FOG             GENMASK(23, 22)
#define PVR_TA_PM2_CLAMP           BIT(21)
#define PVR_TA_PM2_ALPHA           BIT(20)
#define PVR_TA_PM2_TXRALPHA        BIT(19)
#define PVR_TA_PM2_UVFLIP          GENMASK(18, 17)
#define PVR_TA_PM2_UVCLAMP         GENMASK(16, 15)
#define PVR_TA_PM2_FILTER          GENMASK(14, 13)
#define PVR_TA_PM2_MIPBIAS         GENMASK(11, 8)
#define PVR_TA_PM2_TXRENV          GENMASK(7, 6)
#define PVR_TA_PM2_USIZE           GENMASK(5, 3)
#define PVR_TA_PM2_VSIZE           GENMASK(2, 0)
#define PVR_TA_PM3_MIPMAP          BIT(31)
#define PVR_TA_PM3_TXRFMT          GENMASK(30, 21)

/* ======================================================== init / scene */

#define PVR_BINSIZE_0   0
#define PVR_BINSIZE_8   8
#define PVR_BINSIZE_16  16
#define PVR_BINSIZE_32  32

typedef struct {
    int     opb_sizes[5];
    int     vertex_buf_size;
    int     dma_enabled;
    int     fsaa_enabled;
    int     autosort_disabled;
    int     opb_overflow_count;
    int     vbuf_doublebuf_disabled;
} pvr_init_params_t;

int pvr_init(const pvr_init_params_t *params);
int pvr_init_defaults(void);
int pvr_shutdown(void);

void pvr_scene_begin(void);
/* Render this scene into texture RAM at `txr` -- render_w x render_h
 * RGB565 pixels, stride_px pixels a row -- instead of onto the screen.
 * -1 on the arguments KOS refuses (stride below the width or not a
 * multiple of 4). */
int pvr_scene_begin_rtt(pvr_ptr_t txr, uint32_t render_w,
                        uint32_t render_h, uint32_t stride_px);
int pvr_list_begin(pvr_list_t list);
int pvr_list_finish(void);
int pvr_prim(const void *data, size_t size);
int pvr_scene_finish(void);
int pvr_wait_ready(void);
int pvr_check_ready(void);
int pvr_wait_render_done(void);

void pvr_poly_compile(pvr_poly_hdr_t *dst, const pvr_poly_cxt_t *src);
void pvr_poly_cxt_col(pvr_poly_cxt_t *dst, pvr_list_t list);
void pvr_poly_cxt_txr(pvr_poly_cxt_t *dst, pvr_list_t list,
                      int textureformat, int tw, int th, pvr_ptr_t textureaddr,
                      int filtering);

/* ========================================================= registers */

/* The PVR register block. The port writes the punch-through alpha
 * reference and the vertex fog colour (src/gfx/gfx_pvr.c); the backend
 * honours the registers it models and ignores the rest. */
#define PVR_OBJECT_CLIP         0x008c
#define PVR_FOG_TABLE_COLOR     0x00b0
#define PVR_FOG_VERTEX_COLOR    0x00b4
#define PVR_FOG_DENSITY         0x00b8
#define PVR_TEXTURE_MODULO      0x00e4
#define PVR_SCALER_CFG          0x00f4
#define PVR_PT_ALPHA_REF        0x011c
#define PVR_PALETTE_TABLE_BASE  0x1000

void pvr_xbox_reg_write(uint32_t reg, uint32_t value);
uint32_t pvr_xbox_reg_read(uint32_t reg);
#define PVR_SET(REG, VALUE) pvr_xbox_reg_write((REG), (VALUE))
#define PVR_GET(REG)        pvr_xbox_reg_read(REG)

/* KOS: four floats in 0..1 to a packed ARGB word */
#define PVR_PACK_COLOR(a, r, g, b) ( \
    ((uint32_t)(uint8_t)((a) * 255) << 24) | \
    ((uint32_t)(uint8_t)((r) * 255) << 16) | \
    ((uint32_t)(uint8_t)((g) * 255) << 8) | \
    ((uint32_t)(uint8_t)((b) * 255) << 0))

/* ================================================ the TA input and DR
 * The TA's input FIFO (KOS's address); see dc/sq.h for how blocks get to
 * it. The direct-render API (KOS dc/pvr/pvr_dr.h): pvr_dr_target hands out
 * one of two 32-byte store-queue slots in turn, pvr_dr_commit sends the
 * slot to the TA. */
#define PVR_TA_INPUT 0x10000000u

typedef uint32_t pvr_dr_state_t;

void *pvr_xbox_dr_slot(uint32_t which);
void pvr_xbox_dr_commit(void *addr);

#define pvr_dr_init(vtx_buf_ptr) do { *(vtx_buf_ptr) = 0; } while (0)
#define pvr_dr_target(vtx_buf_ptr) \
    ((pvr_vertex_t *)pvr_xbox_dr_slot((vtx_buf_ptr) ^= 1))
#define pvr_dr_commit(addr) pvr_xbox_dr_commit((void *)(addr))

/* ============================================================ textures */

void pvr_txr_set_stride(size_t texture_width);
size_t pvr_txr_get_stride(void);
void pvr_txr_load(const void *src, pvr_ptr_t dst, size_t count);

#define PVR_TXRLOAD_4BPP            0x01
#define PVR_TXRLOAD_8BPP            0x02
#define PVR_TXRLOAD_16BPP           0x03
#define PVR_TXRLOAD_FMT_MASK        0x0f
#define PVR_TXRLOAD_VQ_LOAD         0x10
#define PVR_TXRLOAD_INVERT_Y        0x20
#define PVR_TXRLOAD_FMT_VQ          0x40
#define PVR_TXRLOAD_FMT_TWIDDLED    0x80
#define PVR_TXRLOAD_FMT_NOTWIDDLE   0x80
#define PVR_TXRLOAD_DMA             0x8000
#define PVR_TXRLOAD_NONBLOCK        0x4000
#define PVR_TXRLOAD_SQ              0x2000

void pvr_txr_load_ex(const void *src, pvr_ptr_t dst,
                     uint32_t w, uint32_t h, uint32_t flags);

/* ============================================================ palettes */

typedef enum pvr_palfmt {
    PVR_PAL_ARGB1555,
    PVR_PAL_RGB565,
    PVR_PAL_ARGB4444,
    PVR_PAL_ARGB8888
} pvr_palfmt_t;

void pvr_set_pal_format(pvr_palfmt_t fmt);
void pvr_set_pal_entry(uint32_t idx, uint32_t value);

/* ================================================================ misc */

void pvr_set_bg_color(float r, float g, float b);
void pvr_set_zclip(float zc);
int pvr_get_vbl_count(void);

typedef struct pvr_stats {
    uint64_t frame_last_time;
    uint64_t reg_last_time;
    uint64_t rnd_last_time;
    uint64_t buf_last_time;
    size_t   frame_count;
    size_t   vbl_count;
    size_t   vtx_buffer_used;
    size_t   vtx_buffer_used_max;
    float    frame_rate;
    uint32_t enabled_list_mask;
} pvr_stats_t;

int pvr_get_stats(pvr_stats_t *stat);

__END_DECLS

#endif /* XBOX_DC_PVR_H */
