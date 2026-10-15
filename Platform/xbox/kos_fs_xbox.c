/* kos_fs_xbox.c -- KallistiOS's VFS calls over Xbox files. See kos/fs.h for
 * the mount points.
 *
 * Handles are small integers into a table, as in KOS (FILEHND_INVALID is
 * -1). The table is guarded by a lock: the port's asset reads, its
 * read-ahead thread and its memory-card writer can all open files. */
#include "xbox_xtl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kos/fs.h>
#include <kos/dbglog.h>

#define FS_MAX 64

/* D: is the running XBE's own directory, on disc and on a hard-disk copy
 * alike; the data the Dreamcast build puts on its disc sits beside the
 * XBE in data\ (rxdk.project.json deployPaths). */
#define XBOX_DATA_ROOT "D:\\data"

#define VMU_BLOCK 512

const char *xbox_vmu_save_dir(void);   /* kos_vmu_xbox.c */

typedef struct
{
    int used;
    int is_dir;
    int is_vmu;                 /* a VMU file: whole 512-byte blocks */
    int written;
    HANDLE h;                   /* file handle, or FindFirstFile's */
    WIN32_FIND_DATAA fd;        /* the directory entry not yet returned */
    int fd_pending;
    dirent_t de;
} FsSlot;

static FsSlot sSlots[FS_MAX];
static CRITICAL_SECTION sLock;
static volatile LONG sLockState;

static void lock(void)
{
    if (sLockState != 2)
    {
        if (InterlockedCompareExchange((LONG *)&sLockState, 1, 0) == 0)
        {
            InitializeCriticalSection(&sLock);
            sLockState = 2;
        }
        while (sLockState != 2)
            Sleep(1);
    }
    EnterCriticalSection(&sLock);
}

static void unlock(void)
{
    LeaveCriticalSection(&sLock);
}

/* "/vmu/<port><unit>/<name>" -> the save game's <name>; 1 when it is one */
static int vmu_translate(const char *kos_path, char *out, size_t size)
{
    const char *dir, *name;

    if (strncmp(kos_path, "/vmu/", 5) != 0)
        return 0;
    name = strchr(kos_path + 5, '/');
    dir = xbox_vmu_save_dir();
    if (name == NULL || name[1] == '\0' || dir == NULL)
        return -1;
    if ((size_t)snprintf(out, size, "%s%s", dir, name + 1) >= size)
        return -1;
    return 1;
}

int xbox_fs_translate(const char *kos_path, char *out, size_t size)
{
    const char *rest;
    size_t n;
    char *p;
    int vmu;

    if (kos_path == NULL)
        return -1;
    vmu = vmu_translate(kos_path, out, size);
    if (vmu != 0)
        return vmu < 0 ? -1 : 0;
    if (strncmp(kos_path, "/cd", 3) == 0 && (kos_path[3] == '/' || kos_path[3] == '\0'))
        rest = kos_path + 3;
    else if (strncmp(kos_path, "/rd", 3) == 0 && (kos_path[3] == '/' || kos_path[3] == '\0'))
        rest = kos_path + 3;
    else if (kos_path[0] != '\0' && kos_path[1] == ':')
        rest = NULL;            /* already an Xbox path */
    else
        return -1;              /* /pc, /vmu ... : not on this console */

    if (rest == NULL)
        n = (size_t)snprintf(out, size, "%s", kos_path);
    else
        n = (size_t)snprintf(out, size, "%s%s", XBOX_DATA_ROOT, rest);
    if (n >= size)
        return -1;
    for (p = out; *p; p++)
        if (*p == '/')
            *p = '\\';
    /* "D:\data\" -> "D:\data" for a directory open */
    if (n > 3 && out[n - 1] == '\\')
        out[n - 1] = '\0';
    return 0;
}

static FsSlot *slot_of(file_t f)
{
    if (f < 0 || f >= FS_MAX || !sSlots[f].used)
        return NULL;
    return &sSlots[f];
}

static file_t slot_take(void)
{
    int i;

    for (i = 0; i < FS_MAX; i++)
    {
        if (!sSlots[i].used)
        {
            memset(&sSlots[i], 0, sizeof(sSlots[i]));
            sSlots[i].used = 1;
            sSlots[i].h = INVALID_HANDLE_VALUE;
            return i;
        }
    }
    return FILEHND_INVALID;
}

file_t fs_open(const char *fn, int mode)
{
    char path[MAX_PATH];
    file_t f;
    FsSlot *s;

    if (xbox_fs_translate(fn, path, sizeof(path)) < 0)
        return FILEHND_INVALID;

    lock();
    f = slot_take();
    if (f == FILEHND_INVALID)
    {
        unlock();
        dbglog(DBG_ERROR, "fs_open: out of handles opening %s\n", fn);
        return FILEHND_INVALID;
    }
    s = &sSlots[f];

    if (mode & O_DIR)
    {
        char pattern[MAX_PATH];

        snprintf(pattern, sizeof(pattern), "%s\\*", path);
        s->h = FindFirstFileA(pattern, &s->fd);
        if (s->h == INVALID_HANDLE_VALUE)
        {
            s->used = 0;
            unlock();
            return FILEHND_INVALID;
        }
        s->is_dir = 1;
        s->fd_pending = 1;
    }
    else
    {
        DWORD access = GENERIC_READ, disp = OPEN_EXISTING;
        int acc = mode & O_ACCMODE;

        if (acc == O_WRONLY)
            access = GENERIC_WRITE;
        else if (acc == O_RDWR)
            access = GENERIC_READ | GENERIC_WRITE;
        if (mode & O_CREAT)
            disp = (mode & O_TRUNC) ? CREATE_ALWAYS : OPEN_ALWAYS;
        else if (mode & O_TRUNC)
            disp = TRUNCATE_EXISTING;
        s->h = CreateFileA(path, access, FILE_SHARE_READ, NULL, disp,
                           FILE_ATTRIBUTE_NORMAL, NULL);
        if (s->h == INVALID_HANDLE_VALUE)
        {
            s->used = 0;
            unlock();
            return FILEHND_INVALID;
        }
        if (mode & O_APPEND)
            SetFilePointer(s->h, 0, NULL, FILE_END);
        /* O_META (a VMU file with its VMS header) is how the port opens
         * its save; the file here always holds the whole package */
        s->is_vmu = strncmp(fn, "/vmu/", 5) == 0;
    }
    unlock();
    return f;
}

int fs_close(file_t hnd)
{
    FsSlot *s;

    lock();
    s = slot_of(hnd);
    if (s == NULL)
    {
        unlock();
        return -1;
    }
    if (s->is_dir)
    {
        FindClose(s->h);
    }
    else
    {
        if (s->is_vmu && s->written)
        {
            /* a VMU stores whole blocks: a written file ends on one */
            DWORD size = GetFileSize(s->h, NULL), put;
            static const uint8_t zeros[VMU_BLOCK];

            if (size != INVALID_FILE_SIZE && (size % VMU_BLOCK) != 0)
            {
                SetFilePointer(s->h, 0, NULL, FILE_END);
                WriteFile(s->h, zeros, VMU_BLOCK - (size % VMU_BLOCK), &put, NULL);
            }
            FlushFileBuffers(s->h);
        }
        CloseHandle(s->h);
    }
    s->used = 0;
    unlock();
    return 0;
}

/* Reads and seeks do not hold the table lock while the kernel works: the
 * slot stays valid until the owner closes it, as with KOS's descriptors. */
static HANDLE file_of(file_t hnd)
{
    FsSlot *s;
    HANDLE h;

    lock();
    s = slot_of(hnd);
    h = (s != NULL && !s->is_dir) ? s->h : INVALID_HANDLE_VALUE;
    unlock();
    return h;
}

ssize_t fs_read(file_t hnd, void *buffer, size_t cnt)
{
    HANDLE h = file_of(hnd);
    DWORD got = 0;

    if (h == INVALID_HANDLE_VALUE)
        return -1;
    if (!ReadFile(h, buffer, (DWORD)cnt, &got, NULL))
        return -1;
    return (ssize_t)got;
}

ssize_t fs_write(file_t hnd, const void *buffer, size_t cnt)
{
    HANDLE h = file_of(hnd);
    DWORD put = 0;
    FsSlot *s;

    if (h == INVALID_HANDLE_VALUE)
        return -1;
    if (!WriteFile(h, buffer, (DWORD)cnt, &put, NULL))
        return -1;
    lock();
    s = slot_of(hnd);
    if (s != NULL)
        s->written = 1;
    unlock();
    return (ssize_t)put;
}

off_t fs_seek(file_t hnd, off_t offset, int whence)
{
    HANDLE h = file_of(hnd);
    DWORD method, r;

    if (h == INVALID_HANDLE_VALUE)
        return -1;
    method = (whence == SEEK_CUR) ? FILE_CURRENT
           : (whence == SEEK_END) ? FILE_END : FILE_BEGIN;
    r = SetFilePointer(h, (LONG)offset, NULL, method);
    if (r == INVALID_SET_FILE_POINTER)
        return -1;
    return (off_t)r;
}

off_t fs_tell(file_t hnd)
{
    return fs_seek(hnd, 0, SEEK_CUR);
}

ssize_t fs_total(file_t hnd)
{
    HANDLE h = file_of(hnd);
    DWORD sz;
    FsSlot *s;
    int vmu;

    if (h == INVALID_HANDLE_VALUE)
        return -1;
    sz = GetFileSize(h, NULL);
    if (sz == INVALID_FILE_SIZE)
        return -1;
    lock();
    s = slot_of(hnd);
    vmu = s != NULL && s->is_vmu;
    unlock();
    /* a VMU file's size is whole blocks (the port checks for 3) */
    if (vmu)
        sz = (sz + VMU_BLOCK - 1) / VMU_BLOCK * VMU_BLOCK;
    return (ssize_t)sz;
}

const dirent_t *fs_readdir(file_t hnd)
{
    FsSlot *s;
    const dirent_t *rv = NULL;

    lock();
    s = slot_of(hnd);
    if (s != NULL && s->is_dir)
    {
        while (s->fd_pending)
        {
            WIN32_FIND_DATAA *fd = &s->fd;
            int skip = strcmp(fd->cFileName, ".") == 0 || strcmp(fd->cFileName, "..") == 0;

            if (!skip)
            {
                memset(&s->de, 0, sizeof(s->de));
                strncpy(s->de.name, fd->cFileName, sizeof(s->de.name) - 1);
                s->de.size = (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                             ? -1 : (int)fd->nFileSizeLow;
                s->de.attr = (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? O_DIR : 0;
                rv = &s->de;
            }
            s->fd_pending = FindNextFileA(s->h, fd) ? 1 : 0;
            if (rv != NULL)
                break;
        }
    }
    unlock();
    return rv;
}

/* KOS maps a file into memory only on the romdisk. The Xbox has no
 * romdisk -- data\ is read like the Dreamcast's /cd -- so the answer is the
 * one /cd gives: NULL, read it instead (src/dc/assetroot.c mmap_locked). */
void *fs_mmap(file_t hnd)
{
    (void)hnd;
    return NULL;
}

int fs_unlink(const char *fn)
{
    char path[MAX_PATH];

    if (xbox_fs_translate(fn, path, sizeof(path)) < 0)
        return -1;
    return DeleteFileA(path) ? 0 : -1;
}

int fs_mkdir(const char *fn)
{
    char path[MAX_PATH];

    if (xbox_fs_translate(fn, path, sizeof(path)) < 0)
        return -1;
    return CreateDirectoryA(path, NULL) ? 0 : -1;
}
