/* kos_log_xbox.c -- dbglog / dbgio on the Xbox.
 *
 * A line is formatted on the caller's thread into a 64 KB ring and handed
 * to the xbdm debug channel (OutputDebugStringA, read with xbWatson) by a
 * thread of its own at the lowest priority. The channel BLOCKS once its
 * buffer fills -- printing from a game thread stalls the frame outright --
 * so no caller ever writes to it. A ring that is full drops the line and
 * counts it; the next line that fits says how many were lost.
 *
 * dbgio_disable() (the release build calls it before anything logs, as on
 * the Dreamcast) turns every line off and starts no thread. */
#include "xbox_xtl.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <kos/dbglog.h>
#include <kos/dbgio.h>

int dbglog_level = DBG_KDEBUG;      /* KOS's default: everything */

static volatile LONG sEnabled = 1;
static volatile LONG sState;        /* 0 fresh, 1 starting, 2 running */
static CRITICAL_SECTION sLock;
static HANDLE sWake;

#define RING_SIZE (64 * 1024)
static char sRing[RING_SIZE];
static unsigned sHead, sTail;       /* bytes written / bytes drained */
static unsigned sDropped;

static DWORD WINAPI log_thread(LPVOID p)
{
    static char line[1024];

    (void)p;
    for (;;)
    {
        unsigned n = 0;

        WaitForSingleObject(sWake, 50);
        for (;;)
        {
            EnterCriticalSection(&sLock);
            n = 0;
            while (sTail != sHead && n < sizeof(line) - 1)
            {
                char c = sRing[sTail % RING_SIZE];

                sTail++;
                line[n++] = c;
                if (c == '\n')
                    break;
            }
            LeaveCriticalSection(&sLock);
            if (n == 0)
                break;
            line[n] = '\0';
            OutputDebugStringA(line);
        }
    }
    return 0;
}

static void log_start(void)
{
    if (sState == 2)
        return;
    if (InterlockedCompareExchange((LONG *)&sState, 1, 0) == 0)
    {
        HANDLE h;

        InitializeCriticalSection(&sLock);
        sWake = CreateEvent(NULL, FALSE, FALSE, NULL);
        h = CreateThread(NULL, 16384, log_thread, NULL, 0, NULL);
        if (h != NULL)
        {
            SetThreadPriority(h, THREAD_PRIORITY_IDLE);
            CloseHandle(h);
        }
        sState = 2;
    }
    while (sState != 2)
        Sleep(1);
}

static void ring_put(const char *s, unsigned n)
{
    unsigned i;

    EnterCriticalSection(&sLock);
    if (RING_SIZE - (sHead - sTail) < n + 64)
    {
        sDropped++;
        LeaveCriticalSection(&sLock);
        return;
    }
    if (sDropped)
    {
        char note[64];
        int k = snprintf(note, sizeof(note), "[log: %u lines dropped]\n", sDropped);

        for (i = 0; i < (unsigned)k; i++)
            sRing[(sHead++) % RING_SIZE] = note[i];
        sDropped = 0;
    }
    for (i = 0; i < n; i++)
        sRing[(sHead++) % RING_SIZE] = s[i];
    LeaveCriticalSection(&sLock);
    SetEvent(sWake);
}

void kos_dbglog_printf(int lvl, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    int n;

    /* dbgio_disable() silences the chatter of a release build, as on the
     * Dreamcast; warnings and worse still go out. They are rare, and on
     * a retail console the channel goes nowhere -- but on a devkit they
     * are the only account of a title that quits. */
    if (!sEnabled && lvl > DBG_WARNING)
        return;
    log_start();
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if (n >= (int)sizeof(buf))
        n = sizeof(buf) - 1;
    ring_put(buf, (unsigned)n);
}

void dbglog_set_level(int level)
{
    dbglog_level = level;
}

void dbgio_enable(void)
{
    sEnabled = 1;
}

void dbgio_disable(void)
{
    sEnabled = 0;
}

/* Wait (briefly) for the ring to drain: before the title leaves for the
 * dashboard, so the last lines of an abort reach xbWatson. */
void kos_log_flush(void)
{
    int i;

    if (sState != 2)
        return;
    for (i = 0; i < 100 && sTail != sHead; i++)
    {
        SetEvent(sWake);
        Sleep(10);
    }
}
