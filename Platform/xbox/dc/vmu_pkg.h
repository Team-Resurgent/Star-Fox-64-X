/* dc/vmu_pkg.h -- Xbox shim: the VMS package (header, icons, eyecatch,
 * payload, CRC) a Dreamcast save file is wrapped in. The port keeps its
 * save files in this format on the Xbox too, so a card image written by
 * either console reads on the other; the builder and parser are
 * KallistiOS's (Platform/xbox/kos_vmu_pkg.c). */
#ifndef XBOX_DC_VMU_PKG_H
#define XBOX_DC_VMU_PKG_H

#include <kos/cdefs.h>

__BEGIN_DECLS

typedef struct vmu_pkg {
    char            desc_short[20];
    char            desc_long[36];
    char            app_id[20];
    int             icon_cnt;
    int             icon_anim_speed;
    int             eyecatch_type;
    int             data_len;
    uint16_t        icon_pal[16];
    uint8_t         *icon_data;
    uint8_t         *eyecatch_data;
    const uint8_t   *data;
} vmu_pkg_t;

typedef struct vmu_hdr {
    char        desc_short[16];
    char        desc_long[32];
    char        app_id[16];
    uint16_t    icon_cnt;
    uint16_t    icon_anim_speed;
    uint16_t    eyecatch_type;
    uint16_t    crc;
    uint32_t    data_len;
    uint8_t     reserved[20];
    uint16_t    icon_pal[16];
} vmu_hdr_t;

_Static_assert(sizeof(vmu_hdr_t) == 128, "vmu_hdr_t layout");

#define VMUPKG_EC_NONE      0
#define VMUPKG_EC_16BIT     1
#define VMUPKG_EC_256COL    2
#define VMUPKG_EC_16COL     3

int vmu_pkg_build(vmu_pkg_t *src, uint8_t **dst, int *dst_size);
int vmu_pkg_parse(uint8_t *data, size_t data_size, vmu_pkg_t *pkg);
/* a 32x32 4-bit .ico into the preallocated icon frames and palette */
int vmu_pkg_load_icon(vmu_pkg_t *pkg, const char *icon_fn);

__END_DECLS

#endif /* XBOX_DC_VMU_PKG_H */
