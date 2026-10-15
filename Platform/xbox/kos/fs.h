/* kos/fs.h -- Xbox shim: KallistiOS's VFS calls over Xbox files.
 *
 * Paths keep their KOS names; Platform/xbox/kos_fs_xbox.c maps the mount
 * points onto the Xbox:
 *
 *   /cd/...  ->  D:\data\...   the game's data, staged beside default.xbe
 *                               (D: is the running XBE's own directory, on
 *                               disc and on a hard-disk install alike)
 *   /rd/...  ->  D:\data\...   the romdisk: the Xbox build links none and
 *                               ships those files in the same folder
 *   D:/...   ->  D:\...        an Xbox path, used as it is (the Star Fox
 *                               port's data prefix is "D:/data")
 *   /vmu/a1/name -> the title's save game on U:\ (kos_vmu_xbox.c); a VMU
 *                               file is whole 512-byte blocks long
 *
 * Same values as KOS: file_t is an int, FILEHND_INVALID is -1, O_DIR and
 * O_META are KOS's own open-mode bits on top of <fcntl.h>'s. */
#ifndef XBOX_KOS_FS_H
#define XBOX_KOS_FS_H

#include <kos/cdefs.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>

__BEGIN_DECLS

typedef int file_t;
#define FILEHND_INVALID ((file_t)-1)

#ifndef NAME_MAX
#define NAME_MAX 256
#endif

typedef struct kos_dirent {
    int size;
    char name[NAME_MAX];
    time_t time;
    uint32_t attr;
} dirent_t;

#define O_MODE_MASK 0x0f
#ifndef O_ASYNC
#define O_ASYNC     0x0200
#endif
#define O_DIR       0x1000
#define O_META      0x2000

file_t fs_open(const char *fn, int mode);
int fs_close(file_t hnd);
ssize_t fs_read(file_t hnd, void *buffer, size_t cnt);
ssize_t fs_write(file_t hnd, const void *buffer, size_t cnt);
off_t fs_seek(file_t hnd, off_t offset, int whence);
off_t fs_tell(file_t hnd);
ssize_t fs_total(file_t hnd);
const dirent_t *fs_readdir(file_t hnd);
int fs_unlink(const char *fn);
void *fs_mmap(file_t hnd);
int fs_mkdir(const char *fn);

/* The Xbox path a KOS path maps to (above), into out[size]. 0 on success. */
int xbox_fs_translate(const char *kos_path, char *out, size_t size);

__END_DECLS

#endif /* XBOX_KOS_FS_H */
