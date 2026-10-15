/* kos_vmu_pkg.c -- VMS packages: KallistiOS's vmu_pkg_build and
 * vmu_pkg_parse (kernel/arch/dreamcast/util/vmu_pkg.c, (c)2002 Megan
 * Potter) with the CRC they use (kernel/net/net_crc.c net_crc16ccitt),
 * under the KOS licence (new BSD; see LICENSE.KOS in KallistiOS).
 *
 * The port wraps its save file in this format on the Xbox too, so the file
 * in the Xbox save game is byte-for-byte the file a VMU would hold
 * (src/ultra_reimpl.c osEepromProbe builds it). */
#include <stdlib.h>
#include <string.h>

#include <dc/vmu_pkg.h>
#include <kos/dbglog.h>

static uint16_t net_crc16ccitt(const uint8_t *data, int size, uint16_t start)
{
    uint16_t rv = start, tmp;

    while (size--)
    {
        tmp = (rv >> 8) ^ *data++;
        tmp ^= tmp >> 4;
        rv = (rv << 8) ^ (tmp << 12) ^ (tmp << 5) ^ tmp;
    }
    return rv;
}

static int vmu_eyecatch_size(int eyecatch_type)
{
    switch (eyecatch_type)
    {
        case VMUPKG_EC_NONE:
            return 0;
        case VMUPKG_EC_16BIT:
            return 72 * 56 * 2;
        case VMUPKG_EC_256COL:
            return 512 + 72 * 56;
        case VMUPKG_EC_16COL:
            return 32 + 72 * 56 / 2;
        default:
            return -1;
    }
}

int vmu_pkg_build(vmu_pkg_t *src, uint8_t **dst, int *dst_size)
{
    uint8_t *out;
    int ec_size, out_size;
    vmu_hdr_t *hdr;

    if (src == NULL || dst == NULL)
        return -1;
    if (src->data_len < 0)
        return -2;

    out_size = sizeof(vmu_hdr_t) + 512 * src->icon_cnt + src->data_len;
    ec_size = vmu_eyecatch_size(src->eyecatch_type);
    if (ec_size < 0)
        return -1;
    out_size += ec_size;

    out = malloc(out_size);
    if (!out)
        return -1;
    *dst_size = out_size;
    *dst = out;

    memset(out, 0, out_size);
    hdr = (vmu_hdr_t *)out;
    memset(hdr->desc_short, 32, sizeof(hdr->desc_short));
    memset(hdr->desc_long, 32, sizeof(hdr->desc_long));
    memcpy(hdr->desc_short, src->desc_short, strlen(src->desc_short));
    memcpy(hdr->desc_long, src->desc_long, strlen(src->desc_long));
    strcpy(hdr->app_id, src->app_id);
    hdr->icon_cnt = src->icon_cnt;
    hdr->icon_anim_speed = src->icon_anim_speed;
    hdr->eyecatch_type = src->eyecatch_type;
    hdr->crc = 0;
    hdr->data_len = src->data_len;
    memcpy(hdr->icon_pal, src->icon_pal, sizeof(src->icon_pal));
    out += sizeof(vmu_hdr_t);

    memcpy(out, src->icon_data, 512 * src->icon_cnt);
    out += 512 * src->icon_cnt;
    memcpy(out, src->eyecatch_data, ec_size);
    out += ec_size;
    if (src->data)
        memcpy(out, src->data, src->data_len);
    out += src->data_len;

    out = *dst;
    hdr->crc = net_crc16ccitt(out, out_size, 0);
    return 0;
}

int vmu_pkg_parse(uint8_t *data, size_t data_size, vmu_pkg_t *pkg)
{
    uint16_t crc, crc_save;
    int ec_size, hdr_size, total_size, icon_size;
    vmu_hdr_t *hdr;

    if (data == NULL || pkg == NULL || data_size < sizeof(vmu_hdr_t))
        return -1;
    hdr = (vmu_hdr_t *)data;

    icon_size = 512 * hdr->icon_cnt;
    ec_size = vmu_eyecatch_size(hdr->eyecatch_type);
    hdr_size = sizeof(vmu_hdr_t) + icon_size + ec_size;
    total_size = hdr->data_len + hdr_size;

    if (ec_size < 0 || total_size < 0 || (size_t)total_size > data_size)
    {
        dbglog(DBG_ERROR, "vmu_pkg_parse: file header is corrupted, or no header?\n");
        return -1;
    }

    crc_save = hdr->crc;
    hdr->crc = 0;
    crc = net_crc16ccitt(data, total_size, 0);
    hdr->crc = crc_save;
    if (crc_save != crc)
    {
        dbglog(DBG_ERROR, "vmu_pkg_parse: expected CRC %04x, got %04x\n", crc_save, crc);
        return -1;
    }

    pkg->icon_cnt = hdr->icon_cnt;
    pkg->icon_anim_speed = hdr->icon_anim_speed;
    pkg->eyecatch_type = hdr->eyecatch_type;
    pkg->data_len = hdr->data_len;
    memcpy(pkg->icon_pal, hdr->icon_pal, sizeof(hdr->icon_pal));
    pkg->icon_data = data + sizeof(vmu_hdr_t);
    pkg->eyecatch_data = data + sizeof(vmu_hdr_t) + icon_size;
    pkg->data = data + hdr_size;
    memcpy(pkg->desc_short, hdr->desc_short, sizeof(hdr->desc_short));
    memcpy(pkg->desc_long, hdr->desc_long, sizeof(hdr->desc_long));
    memcpy(pkg->app_id, hdr->app_id, sizeof(hdr->app_id));
    pkg->desc_short[sizeof(hdr->desc_short)] = '\0';
    pkg->desc_long[sizeof(hdr->desc_long)] = '\0';
    pkg->app_id[sizeof(hdr->app_id)] = '\0';
    return 0;
}

/* ================================================ vmu_pkg_load_icon
 * KOS util/vmu_pkg.c: a 32x32, 4-bit Windows .ico, each frame turned into
 * a VMU icon frame (bottom-up rows flipped, the AND mask's transparent
 * pixels to index 15) and the frames' colours merged into one 15-colour
 * ARGB4444 palette, index 15 transparent. The Star Fox port loads its
 * sf64.ico (beside the data, D:\data) into its save file's icon. */

#include <kos/fs.h>

struct ico_header
{
    uint16_t resv;
    uint16_t type;
    uint16_t nb_images;
};

struct ico_dir
{
    uint8_t width;
    uint8_t height;
    uint8_t nb_colors;
    uint8_t resv;
    uint16_t nb_planes;
    uint16_t bpp;
    uint32_t size;
    uint32_t offset;
};

struct bmp_dib_header
{
    uint32_t hdr_size;
    int32_t width;
    int32_t height;
    uint16_t nb_planes;
    uint16_t bpp;
    uint32_t comp;
    uint32_t size;
    uint32_t hppm;
    uint32_t vppm;
    uint32_t nb_colors;
    uint32_t important_colors;
};

_Static_assert(sizeof(struct ico_header) == 6, "ico_header layout");
_Static_assert(sizeof(struct ico_dir) == 16, "ico_dir layout");
_Static_assert(sizeof(struct bmp_dib_header) == 40, "bmp_dib_header layout");

static unsigned int pal_get_map(uint32_t *pal, const uint32_t *curr_pal, uint8_t *map,
                                unsigned int nb_colors)
{
    unsigned int i, j;

    for (i = 0; i < 16; i++)
    {
        for (j = 0; j < nb_colors; j++)
            if (curr_pal[i] == pal[j])
                break;
        if (j < nb_colors)
        {
            map[i] = (uint8_t)j;
            continue;
        }
        /* 15 at most: the 16th index is the transparent one */
        if (nb_colors == 15)
            return 0;
        pal[nb_colors] = curr_pal[i];
        map[i] = (uint8_t)nb_colors++;
    }
    return nb_colors;
}

static uint16_t argb8888_to_argb4444(uint32_t px)
{
    return (uint16_t)(0xf000 | ((px >> 12) & 0x0f00) | ((px >> 8) & 0x00f0) | ((px >> 4) & 0x000f));
}

int vmu_pkg_load_icon(vmu_pkg_t *pkg, const char *icon_fn)
{
    uint8_t px, pxh, pxl, pal_map[16];
    unsigned int i, j, x, y, nb_colors = 0;
    struct bmp_dib_header dib;
    uint32_t palette[16], curr_palette[16];
    struct ico_header hdr;
    struct ico_dir dir;
    uint8_t and_mask;
    uint8_t frame[512];
    file_t fd;

    if (!pkg->icon_cnt || !pkg->icon_data)
    {
        dbglog(DBG_ERROR, "vmu_pkg_load_icon: vmu_pkg_t icon not preallocated\n");
        return -1;
    }
    fd = fs_open(icon_fn, O_RDONLY);
    if (fd == FILEHND_INVALID)
        return fd;

    fs_read(fd, &hdr, sizeof(hdr));
    if (hdr.resv != 0 || hdr.type != 1)
    {
        dbglog(DBG_ERROR, "vmu_pkg_load_icon: Invalid .ico header\n");
        goto out_err_close;
    }
    if (hdr.nb_images < pkg->icon_cnt)
        pkg->icon_cnt = hdr.nb_images;
    if (hdr.nb_images > pkg->icon_cnt)
        dbglog(DBG_WARNING, "vmu_pkg_load_icon: .ico file has %u frames but we only have space for %u\n",
               hdr.nb_images, pkg->icon_cnt);

    for (i = 0; i < hdr.nb_images; i++)
    {
        fs_read(fd, &dir, sizeof(dir));
        if (dir.width != 32 || dir.height != 32 || dir.bpp != 4)
        {
            dbglog(DBG_ERROR, "vmu_pkg_load_icon: Invalid .ico width, height or bpp\n");
            goto out_err_close;
        }
    }

    for (i = 0; i < (unsigned int)pkg->icon_cnt; i++)
    {
        fs_read(fd, &dib, sizeof(dib));
        if (dib.hdr_size != 40)
        {
            dbglog(DBG_ERROR, "vmu_pkg_load_icon: Invalid DIB header for frame %u\n", i);
            goto out_err_close;
        }
        if (dib.comp != 0)
        {
            dbglog(DBG_ERROR, "vmu_pkg_load_icon: Only uncompressed .ico are supported.\n");
            goto out_err_close;
        }
        fs_read(fd, curr_palette, sizeof(curr_palette));
        nb_colors = pal_get_map(palette, curr_palette, pal_map, nb_colors);
        if (!nb_colors)
        {
            dbglog(DBG_ERROR, "vmu_pkg_load_icon: .ico has > 15 colors.\n");
            goto out_err_close;
        }
        fs_read(fd, frame, sizeof(frame));
        for (y = 0; y < 32; y++)
        {
            for (x = 0; x < 16; x += 4)
            {
                fs_read(fd, &and_mask, sizeof(and_mask));
                for (j = 0; j < 4; j++)
                {
                    px = frame[y * 16 + x + j];
                    pxh = px >> 4;
                    pxl = px & 0x0f;
                    if (and_mask & (1u << (7 - j * 2)))
                        pxh = 15;
                    else
                        pxh = pal_map[pxh];
                    if (and_mask & (1u << (6 - j * 2)))
                        pxl = 15;
                    else
                        pxl = pal_map[pxl];
                    pkg->icon_data[i * 512 + 496 - 16 * y + x + j] = (uint8_t)((pxh << 4) | pxl);
                }
            }
        }
    }

    for (i = 0; i < nb_colors; i++)
        pkg->icon_pal[i] = argb8888_to_argb4444(palette[i]);
    pkg->icon_pal[15] = 0x0;    /* transparent */
    fs_close(fd);
    return 0;

out_err_close:
    fs_close(fd);
    return -1;
}
