/* kos_vmu_xbox.c -- the Dreamcast memory card on the Xbox: a VMU whose
 * files live in the title's save game on the hard disk.
 *
 * The Star Fox port saves its cartridge EEPROM to a VMU file,
 * /vmu/<port><unit>/sf64.rec, a VMS package (Platform/xbox/kos_vmu_pkg.c)
 * opened through the file system with O_META (src/ultra_reimpl.c). On the
 * Xbox the card is one save game, created with XCreateSaveGame on U:\ --
 * the dashboard's own save area, as the Super Mario World, Zelda and Mario
 * Kart 64 ports do -- and each VMU file is a file of the same name in it,
 * holding the same bytes a VMU would; kos_fs_xbox.c opens them there. The
 * vmufs calls below work on the same files.
 *
 * The card behaves as a freshly formatted 200-block VMU that holds only
 * this game's files: the root block reads as formatted, a file takes
 * ceil(size / 512) blocks, and free blocks are 200 less those -- or fewer,
 * when the hard disk itself has less room than that. The save game is made
 * on the first look at the card; if the dashboard cannot make one (a full
 * disk), the card does not answer, which the port treats as a dead card. */
#include "xbox_xtl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dc/maple.h>
#include <dc/maple/vmu.h>
#include <dc/vmufs.h>
#include <kos/dbglog.h>

#define VMU_BLOCKS      200     /* a VMU's user area */
#define VMU_BLOCK_SIZE  512
#define VMU_ROOT_BLOCK  255

static CRITICAL_SECTION sLock;
static volatile LONG sLockState;
static char sDir[MAX_PATH];         /* the save game's folder, '\' at the end */
static int sDirState;               /* 0 untried, 1 ready, -1 failed */

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

static int is_card(maple_device_t *dev)
{
    return dev != NULL && (dev->info.functions & MAPLE_FUNC_MEMCARD) != 0;
}

/* The save game's folder, made on first use. Locked by the caller. */
static const char *save_dir(void)
{
    DWORD rv;
    size_t n;

    if (sDirState)
        return sDirState > 0 ? sDir : NULL;
    rv = XCreateSaveGame("U:\\", L"Star Fox 64", OPEN_ALWAYS, 0, sDir, sizeof(sDir));
    if (rv != ERROR_SUCCESS)
    {
        sDirState = -1;
        return NULL;
    }
    n = strlen(sDir);
    if (n && sDir[n - 1] != '\\' && n + 1 < sizeof(sDir))
    {
        sDir[n] = '\\';
        sDir[n + 1] = '\0';
    }
    sDirState = 1;
    return sDir;
}

/* The save game's folder for the file system's /vmu paths (kos_fs_xbox.c),
 * '\' at the end; NULL when the dashboard could not make one (a full
 * disk), which reads as no card. */
const char *xbox_vmu_save_dir(void)
{
    const char *d;

    lock();
    d = save_dir();
    unlock();
    return d;
}

/* The save game's own files, which are the dashboard's or the port's
 * settings (xbox_settings.c), not the card's. */
static int is_meta(const char *name)
{
    return _stricmp(name, "SaveMeta.xbx") == 0 || _stricmp(name, "SaveImage.xbx") == 0 ||
           _stricmp(name, "sf64x.ini") == 0 || strstr(name, ".new") != NULL;
}

static uint8_t bcd(int v)
{
    return (uint8_t)(((v / 10) % 10) << 4 | (v % 10));
}

static void stamp_of(const FILETIME *ft, vmu_timestamp_t *ts)
{
    FILETIME local;
    SYSTEMTIME st;

    memset(ts, 0, sizeof(*ts));
    if (!FileTimeToLocalFileTime(ft, &local) || !FileTimeToSystemTime(&local, &st))
        return;
    ts->cent = bcd(st.wYear / 100);
    ts->year = bcd(st.wYear % 100);
    ts->month = bcd(st.wMonth);
    ts->day = bcd(st.wDay);
    ts->hour = bcd(st.wHour);
    ts->min = bcd(st.wMinute);
    ts->sec = bcd(st.wSecond);
    /* VMU: 0 = Monday; SYSTEMTIME: 0 = Sunday */
    ts->dow = (uint8_t)((st.wDayOfWeek + 6) % 7);
}

static int blocks_of(DWORD size)
{
    return (int)((size + VMU_BLOCK_SIZE - 1) / VMU_BLOCK_SIZE);
}

/* The card's files, counted. Locked by the caller. -1 when the save game
 * cannot be read. */
static int list(vmu_dir_t *out, int max, int *used_blocks)
{
    const char *dir = save_dir();
    char pattern[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    int n = 0, used = 0;

    if (dir == NULL)
        return -1;
    snprintf(pattern, sizeof(pattern), "%s*", dir);
    h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || is_meta(fd.cFileName) ||
                strlen(fd.cFileName) > 12)
                continue;
            used += blocks_of(fd.nFileSizeLow);
            if (out != NULL && n < max)
            {
                vmu_dir_t *d = &out[n];

                memset(d, 0, sizeof(*d));
                d->filetype = 0x33;     /* data */
                d->firstblk = (uint16_t)(VMU_BLOCKS - 1 - used);
                memcpy(d->filename, fd.cFileName, strlen(fd.cFileName));
                stamp_of(&fd.ftLastWriteTime, &d->timestamp);
                d->filesize = (uint16_t)blocks_of(fd.nFileSizeLow);
                d->hdroff = 0;
            }
            n++;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    if (used_blocks)
        *used_blocks = used;
    return n;
}

int vmu_block_read(maple_device_t *dev, uint16_t blocknum, uint8_t *buffer)
{
    int ok;

    if (!is_card(dev) || buffer == NULL)
        return MAPLE_EINVALID;
    lock();
    ok = save_dir() != NULL;
    unlock();
    if (!ok)
        return MAPLE_ETIMEOUT;      /* no answer: the card is not there */
    memset(buffer, 0, VMU_BLOCK_SIZE);
    if (blocknum == VMU_ROOT_BLOCK)
    {
        /* a formatted card's root block starts with sixteen 0x55 */
        memset(buffer, 0x55, 16);
        buffer[0x46] = VMU_ROOT_BLOCK - 1;  /* FAT location, as a VMU has it */
        buffer[0x48] = 1;
        buffer[0x4a] = 253;                 /* directory location */
        buffer[0x4c] = 13;
        buffer[0x50] = VMU_BLOCKS;          /* user blocks */
    }
    return MAPLE_EOK;
}

int vmu_block_write(maple_device_t *dev, uint16_t blocknum, const uint8_t *buffer)
{
    /* nothing in the port writes raw blocks */
    (void)dev;
    (void)blocknum;
    (void)buffer;
    return MAPLE_ENOTSUPP;
}

int vmufs_readdir(maple_device_t *dev, vmu_dir_t **outbuf, int *outcnt)
{
    vmu_dir_t *d;
    int n;

    if (!is_card(dev) || outbuf == NULL || outcnt == NULL)
        return -1;
    lock();
    n = list(NULL, 0, NULL);
    if (n < 0)
    {
        unlock();
        return -1;
    }
    d = (vmu_dir_t *)calloc(n > 0 ? n : 1, sizeof(*d));
    if (d == NULL)
    {
        unlock();
        return -1;
    }
    n = list(d, n, NULL);
    unlock();
    *outbuf = d;
    *outcnt = n < 0 ? 0 : n;
    return 0;
}

static int read_file(const char *name, void **outbuf, int *outsize)
{
    const char *dir = save_dir();
    char path[MAX_PATH];
    HANDLE h;
    DWORD size, got = 0;
    uint8_t *buf;

    if (dir == NULL)
        return -1;
    snprintf(path, sizeof(path), "%s%s", dir, name);
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    size = GetFileSize(h, NULL);
    /* a VMU hands back whole blocks */
    buf = (uint8_t *)calloc(1, (size_t)blocks_of(size) * VMU_BLOCK_SIZE + 1);
    if (buf == NULL || size == INVALID_FILE_SIZE ||
        !ReadFile(h, buf, size, &got, NULL) || got != size)
    {
        free(buf);
        CloseHandle(h);
        return -1;
    }
    CloseHandle(h);
    *outbuf = buf;
    *outsize = blocks_of(size) * VMU_BLOCK_SIZE;
    return 0;
}

int vmufs_read(maple_device_t *dev, const char *fn, void **outbuf, int *outsize)
{
    int rv;

    if (!is_card(dev) || fn == NULL || outbuf == NULL || outsize == NULL)
        return -1;
    lock();
    rv = read_file(fn, outbuf, outsize);
    unlock();
    return rv;
}

int vmufs_read_dirent(maple_device_t *dev, vmu_dir_t *dirent, void **outbuf, int *outsize)
{
    char name[13];

    if (dirent == NULL)
        return -1;
    memcpy(name, dirent->filename, 12);
    name[12] = '\0';
    return vmufs_read(dev, name, outbuf, outsize);
}

/* Written to "<name>.new" and then moved over the old file, so a power cut
 * mid-write leaves the previous save intact -- the VMU's own guarantee is
 * per block; this keeps the whole file. */
int vmufs_write(maple_device_t *dev, const char *fn, void *inbuf, int insize, int flags)
{
    const char *dir;
    char path[MAX_PATH], tmp[MAX_PATH];
    HANDLE h;
    DWORD put = 0;
    int ok;

    if (!is_card(dev) || fn == NULL || strlen(fn) > 12 || inbuf == NULL || insize < 0)
        return -1;
    lock();
    dir = save_dir();
    if (dir == NULL)
    {
        unlock();
        return -1;
    }
    snprintf(path, sizeof(path), "%s%s", dir, fn);
    snprintf(tmp, sizeof(tmp), "%s%s.new", dir, fn);
    if (!(flags & VMUFS_OVERWRITE) && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
    {
        unlock();
        return -1;
    }
    h = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        unlock();
        return -1;
    }
    ok = WriteFile(h, inbuf, (DWORD)insize, &put, NULL) && put == (DWORD)insize;
    ok = FlushFileBuffers(h) && ok;
    CloseHandle(h);
    if (ok)
    {
        DeleteFileA(path);
        ok = MoveFileA(tmp, path) != 0;
    }
    if (!ok)
        DeleteFileA(tmp);
    unlock();
    return ok ? 0 : -1;
}

int vmufs_delete(maple_device_t *dev, const char *fn)
{
    const char *dir;
    char path[MAX_PATH];
    int ok;

    if (!is_card(dev) || fn == NULL)
        return -1;
    lock();
    dir = save_dir();
    ok = 0;
    if (dir != NULL)
    {
        snprintf(path, sizeof(path), "%s%s", dir, fn);
        ok = DeleteFileA(path) != 0;
    }
    unlock();
    return ok ? 0 : -1;
}

int vmufs_free_blocks(maple_device_t *dev)
{
    ULARGE_INTEGER avail, total, freeb;
    int used = 0, free_blocks;

    if (!is_card(dev))
        return -1;
    lock();
    if (list(NULL, 0, &used) < 0)
    {
        unlock();
        return -1;
    }
    unlock();
    free_blocks = VMU_BLOCKS - used;
    if (free_blocks < 0)
        free_blocks = 0;
    if (GetDiskFreeSpaceExA("U:\\", &avail, &total, &freeb))
    {
        ULONGLONG disk = avail.QuadPart / VMU_BLOCK_SIZE;

        if (disk < (ULONGLONG)free_blocks)
            free_blocks = (int)disk;
    }
    return free_blocks;
}
