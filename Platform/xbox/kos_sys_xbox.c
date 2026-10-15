/* kos_sys_xbox.c -- KallistiOS's system services on the Xbox: the clocks,
 * the program break and memory statistics, interrupt masking, the RTC,
 * abort/exit, and the store-queue fill. See Platform/xbox/kos.h. */
#include "xbox_xtl.h"

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arch/arch.h>
#include <arch/irq.h>
#include <arch/rtc.h>
#include <arch/stack.h>
#include <arch/timer.h>
#include <dc/sq.h>
#include <kos/dbglog.h>
#include <kos/mm.h>

/* ================================================================ clocks
 * QueryPerformanceCounter: the ACPI timer behind it is unaffected by the
 * RDTSC rewriting some upgraded-CPU BIOSes do at load (see the Melee-X
 * notes), and its frequency is fixed. */

static LARGE_INTEGER sFreq;

static uint64_t qpc_now(void)
{
    LARGE_INTEGER now;

    if (sFreq.QuadPart == 0)
    {
        QueryPerformanceFrequency(&sFreq);
        if (sFreq.QuadPart == 0)
            sFreq.QuadPart = 3375000;   /* the ACPI timer's rate */
    }
    QueryPerformanceCounter(&now);
    return (uint64_t)now.QuadPart;
}

/* t ticks in units of `per_sec`, split so it cannot overflow */
static uint64_t qpc_scale(uint64_t t, uint64_t per_sec)
{
    uint64_t f = (uint64_t)sFreq.QuadPart;

    return (t / f) * per_sec + ((t % f) * per_sec) / f;
}

uint64_t timer_ns_gettime64(void)
{
    uint64_t t = qpc_now();

    return qpc_scale(t, 1000000000ull);
}

uint64_t timer_us_gettime64(void)
{
    uint64_t t = qpc_now();

    return qpc_scale(t, 1000000ull);
}

uint64_t timer_ms_gettime64(void)
{
    uint64_t t = qpc_now();

    return qpc_scale(t, 1000ull);
}

void timer_spin_sleep(int ms)
{
    uint64_t until = timer_us_gettime64() + (uint64_t)ms * 1000u;

    while (timer_us_gettime64() < until)
        ;
}

/* =================================================== memory and break
 * See kos/mm.h. The Xbox heap is the RTL process heap; "the RAM the heap
 * can still grow into" is the console's free physical memory. */

uint8_t *_arch_mem_top = (uint8_t *)0x7ff00000u;

static size_t free_phys(void)
{
    MEMORYSTATUS ms;

    memset(&ms, 0, sizeof(ms));
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    return (size_t)ms.dwAvailPhys;
}

void *mm_sbrk(ptrdiff_t increment)
{
    if (increment != 0)
    {
        /* nothing in the port grows the break; refuse as sbrk does */
        return (void *)-1;
    }
    return (void *)(_arch_mem_top - THD_KERNEL_STACK_SIZE - free_phys());
}

/* picolibc declares mallinfo; RXDK's heap (libc/xbox/heapalloc.c) does not
 * implement it. The port reads it for its memory logs and for keepcost in
 * the read-ahead budget (src/dc/assetroot.c pf_top). An RTL heap keeps no
 * top-chunk slack the way dlmalloc does, so keepcost is 0 and the budget is
 * free physical memory alone; the in-use figure is the title's committed
 * memory, which is what the logs are after. */
struct mallinfo mallinfo(void)
{
    struct mallinfo mi;
    MEMORYSTATUS ms;

    memset(&mi, 0, sizeof(mi));
    memset(&ms, 0, sizeof(ms));
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    mi.arena = ms.dwTotalPhys;
    mi.uordblks = ms.dwTotalPhys - ms.dwAvailPhys;
    mi.fordblks = ms.dwAvailPhys;
    mi.keepcost = 0;
    return mi;
}

/* ================================================================== irq
 * KOS masks interrupts for a short critical section. On the Xbox's single
 * CPU, DISPATCH_LEVEL is the equivalent: the scheduler cannot switch
 * threads until the level drops again. Every user in the port is a debug
 * facility that is compiled out of this build (arch/irq.h). */

UCHAR __fastcall KfRaiseIrql(UCHAR NewIrql);
VOID __fastcall KfLowerIrql(UCHAR NewIrql);
#define XBOX_DISPATCH_LEVEL 2

irq_mask_t irq_disable(void)
{
    return (irq_mask_t)KfRaiseIrql(XBOX_DISPATCH_LEVEL);
}

void irq_restore(irq_mask_t v)
{
    KfLowerIrql((UCHAR)v);
}

void irq_scoped_cleanup(irq_mask_t *state)
{
    irq_restore(*state);
}

/* ================================================================== rtc */

time_t rtc_unix_secs(void)
{
    FILETIME ft;
    uint64_t t;

    GetSystemTimeAsFileTime(&ft);
    t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    /* 100 ns ticks since 1601 -> seconds since 1970 */
    return (time_t)(t / 10000000ull - 11644473600ull);
}

/* ======================================================= abort / exit */

void kos_log_flush(void);

/* The last word before the dashboard, written straight to the debug
 * channel: the title is ending, so blocking on it costs nothing, and the
 * log thread may never run again. `who` is the caller's return address --
 * look it up in the symbols of out/<Config>/ssb64x.exe, the PE the XBE is
 * made from (nm, or addr2line -f -e). */
static void exit_report(const char *what, void *who)
{
    char line[160];

    kos_log_flush();
    snprintf(line, sizeof(line), "%s: called from %08lx; %lu KB of memory free\n", what,
             (unsigned long)(uintptr_t)who, (unsigned long)(free_phys() / 1024));
    OutputDebugStringA(line);
}

void arch_abort(void)
{
    exit_report("arch_abort: the game stopped itself", __builtin_return_address(0));
    /* KOS halts the machine back to the loader; the Xbox equivalent is the
     * dashboard. (No breakpoint: on a retail console an int 3 nobody is
     * attached to is a fatal-error screen, not a stop.) */
    XLaunchNewImage(NULL, NULL);
    for (;;)
        Sleep(1000);
}

void arch_exit(void)
{
    exit_report("arch_exit: back to the dashboard", __builtin_return_address(0));
    XLaunchNewImage(NULL, NULL);
    for (;;)
        Sleep(1000);
}

void arch_reboot(void)
{
    arch_exit();
}

/* ========================================================= store queue */

/* pvr_xbox.cpp: a write into texture memory has to be seen by its texture
 * cache, the way pvr_txr_load's are */
void pvr_xbox_vram_written(const void *p, size_t n);

void *sq_set32(void *dest, uint32_t c, size_t n)
{
    uint32_t *d = (uint32_t *)dest;
    size_t i;

    for (i = 0; i < n / 4; i++)
        d[i] = c;
    pvr_xbox_vram_written(dest, n);
    return dest;
}

void *sq_cpy(void *dest, const void *src, size_t n)
{
    return memcpy(dest, src, n);
}
