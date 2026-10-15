/* kos_wdt_xbox.c -- KallistiOS's watchdog-timer driver (dc/wdt.h) on the Xbox.
 *
 * KOS's hardware/wdt.c programs the SH4 watchdog peripheral: the 8-bit WTCNT
 * counts up at the divided peripheral clock, and each overflow raises an
 * interrupt (interval mode, always the /32 divider: one overflow per 41 us)
 * whose handler adds 41 us to an elapsed total and calls the user's callback
 * whenever that reaches the requested interval -- or, in watchdog mode,
 * resets the machine.
 *
 * Here the counter is a function of the clock: its value is the count it was
 * last set to plus the time since, in counter steps. Overflows are handled in
 * order by service(), exactly as wdt_isr handles them -- on a time-critical
 * thread (the interrupt outranks every thread on the Dreamcast), and also
 * first thing in every wdt_* call, so the caller always sees the effect of
 * every overflow that fell due before its call, as it would of an interrupt
 * that had already fired. Calls are serialised with the handler by a lock,
 * as masking interrupts does; after wdt_disable() returns, the callback never
 * runs again.
 *
 * irq_prio has no counterpart: there are no interrupt priority levels to set
 * for a thread. Watchdog mode's reset is arch_reboot() (back to the dashboard).
 */
#include "xbox_xtl.h"

#include <stdint.h>

#include <arch/arch.h>
#include <dc/wdt.h>

#define WDT_INT_DEFAULT 41              /* interval-mode period per overflow (us) */

static CRITICAL_SECTION sWdtLock;
static volatile LONG sWdtState;         /* 0 none, 1 starting, 2 ready */
static HANDLE sWdtWake;

static LARGE_INTEGER sFreq;
static int sEnabled;
static int sWatchdog;                   /* overflow resets instead of calling back */
static unsigned sClk;                   /* WDT_CLK_DIV: 256 steps last 41 us << sClk */
static uint64_t sBaseQpc;               /* when the counter was last set ... */
static uint32_t sBaseCount;             /* ... and to what */
static uint64_t sOverflows;             /* overflows since then already handled */
static uint8_t sStopped;                /* WTCNT while stopped */

static wdt_callback sCallback;
static void *sUserData;
static uint32_t sInterval;
static uint32_t sElapsed;

/* counter steps since the counter was last set, plus the count it was set to */
static uint64_t counts_now(void)
{
    LARGE_INTEGER t;
    double us;

    QueryPerformanceCounter(&t);
    us = (double)(uint64_t)(t.QuadPart - (LONGLONG)sBaseQpc) * 1e6 / (double)sFreq.QuadPart;
    return sBaseCount + (uint64_t)(us * 256.0 / (double)(WDT_INT_DEFAULT << sClk));
}

static void rebase(uint8_t count)
{
    LARGE_INTEGER t;

    QueryPerformanceCounter(&t);
    sBaseQpc = (uint64_t)t.QuadPart;
    sBaseCount = count;
    sOverflows = 0;
}

/* wdt_isr, once for each overflow that has fallen due. Lock held. */
static void service(void)
{
    uint64_t over;

    if (!sEnabled)
        return;
    over = counts_now() / 256;
    while (sEnabled && sOverflows < over)
    {
        sOverflows++;
        if (sWatchdog)
            arch_reboot();
        sElapsed += WDT_INT_DEFAULT;
        if (sElapsed >= sInterval)
        {
            sCallback(sUserData);
            sElapsed -= sInterval;
        }
    }
}

static void wdt_lock(void);

static DWORD WINAPI wdt_thread(LPVOID arg)
{
    int on;

    (void)arg;
    for (;;)
    {
        wdt_lock();
        service();
        on = sEnabled;
        LeaveCriticalSection(&sWdtLock);
        if (on)
            Sleep(1);
        else
            WaitForSingleObject(sWdtWake, INFINITE);
    }
    return 0;
}

static void wdt_lock(void)
{
    if (sWdtState != 2)
    {
        if (InterlockedCompareExchange((LONG *)&sWdtState, 1, 0) == 0)
        {
            HANDLE th;

            InitializeCriticalSection(&sWdtLock);
            QueryPerformanceFrequency(&sFreq);
            sWdtWake = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (sWdtWake == NULL)
                arch_abort();
            /* Ready BEFORE the thread exists: raised to time-critical it
             * preempts this one at once and takes the lock itself, and a
             * thread that found the lock still starting would wait on a
             * caller that can never run again. */
            sWdtState = 2;
            th = CreateThread(NULL, 16 * 1024, wdt_thread, NULL, 0, NULL);
            if (th == NULL)
                arch_abort();
            SetThreadPriority(th, THREAD_PRIORITY_TIME_CRITICAL);
            CloseHandle(th);
        }
        /* Sleep(1), not Sleep(0): Sleep(0) yields only to threads of the
         * same or higher priority, so a higher-priority caller here would
         * never let a lower-priority initialiser finish. */
        while (sWdtState != 2)
            Sleep(1);
    }
    EnterCriticalSection(&sWdtLock);
}

static void wdt_unlock(void)
{
    LeaveCriticalSection(&sWdtLock);
}

void wdt_enable_timer(uint8_t initial_count, uint32_t micro_seconds, uint8_t irq_prio,
                      wdt_callback callback, void *user_data)
{
    (void)irq_prio;
    wdt_lock();
    sEnabled = 0;                       /* stop, interval mode, /32 */
    sWatchdog = 0;
    sClk = WDT_CLK_DIV_32;
    sCallback = callback;
    sUserData = user_data;
    sElapsed = 0;
    sInterval = micro_seconds;
    rebase(initial_count);
    sEnabled = 1;
    wdt_unlock();
    SetEvent(sWdtWake);
}

void wdt_enable_watchdog(uint8_t initial_count, WDT_CLK_DIV clk_config, WDT_RST reset_select)
{
    (void)reset_select;                 /* either reset is a return to the dashboard */
    wdt_lock();
    sEnabled = 0;
    sWatchdog = 1;
    sClk = (unsigned)clk_config & 7u;
    rebase(initial_count);
    sEnabled = 1;
    wdt_unlock();
    SetEvent(sWdtWake);
}

void wdt_set_counter(uint8_t count)
{
    wdt_lock();
    service();
    if (sEnabled)
        rebase(count);
    else
        sStopped = count;
    wdt_unlock();
}

uint8_t wdt_get_counter(void)
{
    uint8_t v;

    wdt_lock();
    service();
    v = sEnabled ? (uint8_t)(counts_now() & 0xff) : sStopped;
    wdt_unlock();
    return v;
}

void wdt_pet(void)
{
    wdt_set_counter(0);
}

void wdt_disable(void)
{
    wdt_lock();
    service();
    if (sEnabled)
        sStopped = (uint8_t)(counts_now() & 0xff);
    sEnabled = 0;                       /* stop the WDT */
    sCallback = NULL;                   /* unregister the handler */
    sUserData = NULL;
    wdt_unlock();
    wdt_pet();                          /* reset the counter */
}

int wdt_is_enabled(void)
{
    int on;

    wdt_lock();
    on = sEnabled;
    wdt_unlock();
    return on;
}
