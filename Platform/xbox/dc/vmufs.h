/* dc/vmufs.h -- Xbox shim: the memory card's file system, as KallistiOS
 * presents it, over files in the title's Xbox save game
 * (Platform/xbox/kos_vmu_xbox.c). Struct layouts are KOS's own. */
#ifndef XBOX_DC_VMUFS_H
#define XBOX_DC_VMUFS_H

#include <kos/cdefs.h>
#include <dc/maple.h>
#include <dc/maple/vmu.h>

__BEGIN_DECLS

typedef struct {
    uint8_t cent;
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t dow;
} vmu_timestamp_t;

typedef struct {
    uint8_t         filetype;
    uint8_t         copyprotect;
    uint16_t        firstblk;
    char            filename[12];
    vmu_timestamp_t timestamp;
    uint16_t        filesize;
    uint16_t        hdroff;
    uint8_t         dirty;
    uint8_t         pad1[3];
} vmu_dir_t;

_Static_assert(sizeof(vmu_dir_t) == 32, "vmu_dir_t layout");

#define VMUFS_OVERWRITE 1
#define VMUFS_VMUGAME   2
#define VMUFS_NOCOPY    4

int vmufs_readdir(maple_device_t *dev, vmu_dir_t **outbuf, int *outcnt);
int vmufs_read(maple_device_t *dev, const char *fn, void **outbuf, int *outsize);
int vmufs_read_dirent(maple_device_t *dev, vmu_dir_t *dirent, void **outbuf, int *outsize);
int vmufs_write(maple_device_t *dev, const char *fn, void *inbuf, int insize, int flags);
int vmufs_delete(maple_device_t *dev, const char *fn);
int vmufs_free_blocks(maple_device_t *dev);

__END_DECLS

#endif /* XBOX_DC_VMUFS_H */
