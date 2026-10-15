/* xbox_main.c -- the Xbox entry point.
 *
 * XapiTitleStartup calls `void __cdecl main(void)`. The game's own entry is
 * src/sys/sys_main.c's KallistiOS `int main(int, char **)`, compiled under
 * the name sf64_main; it finds the data, maps the segments, starts the
 * audio thread, the controllers, the renderer and the sound chip through
 * the KallistiOS calls Platform/xbox implements, and then runs the game
 * loop, which does not return. This file only does what has to happen
 * before any of that on the Xbox. */
#include "xbox_xtl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <dc/maple.h>
#include <kos/dbglog.h>
#include <arch/arch.h>

#include "xbox_settings.h"

int sf64_main(int argc, char **argv);
void xbox_fpu_setup(void);

/* The SH-4 runs the game with its FPU flushing denormals to zero (KOS's
 * FPSCR.DN). SSE's flush-to-zero is the same rule for the single-precision
 * maths, which is all of the game's. (The Pentium III has FTZ but not DAZ;
 * setting DAZ there faults.) Every thread that runs game code calls this:
 * main here, the rest in Platform/xbox/kos_thread_xbox.c. */
void xbox_fpu_setup(void)
{
    unsigned int csr;

    __asm__ volatile("stmxcsr %0" : "=m"(csr));
    csr |= 0x8000;      /* FTZ */
    __asm__ volatile("ldmxcsr %0" : : "m"(csr));
}

void kos_log_flush(void);

/* The last word of a title that faults. XAPI runs every thread, main's
 * too, under UnhandledExceptionFilter, whose release build says nothing
 * and hands the fault to the kernel -- and the console simply restarts.
 * This says what faulted and where, straight to the debug channel (the
 * title is ending; blocking costs nothing), and then lets the fault go on
 * as before. The addresses are the title's own: look them up in the
 * symbols of out/<Config>/default.exe, the PE the XBE is made from. The
 * stack words listed are the ones that point into the image, the likely
 * return addresses, nearest first. */
static LONG WINAPI xbox_fault_report(struct _EXCEPTION_POINTERS *ep)
{
    static volatile LONG sOnce;
    const EXCEPTION_RECORD *er = ep->ExceptionRecord;
    const CONTEXT *c = ep->ContextRecord;
    const uint32_t *sp = (const uint32_t *)(uintptr_t)c->Esp;
    char line[256];
    int i, n;

    if (InterlockedExchange((LONG *)&sOnce, 1) != 0)
        return 0;               /* EXCEPTION_CONTINUE_SEARCH */
    kos_log_flush();
    snprintf(line, sizeof(line),
             "xbox: FAULT %08lx at %08lx (info %08lx %08lx), thread %lu\n",
             (unsigned long)er->ExceptionCode, (unsigned long)(uintptr_t)er->ExceptionAddress,
             (unsigned long)(er->NumberParameters > 0 ? er->ExceptionInformation[0] : 0),
             (unsigned long)(er->NumberParameters > 1 ? er->ExceptionInformation[1] : 0),
             (unsigned long)GetCurrentThreadId());
    OutputDebugStringA(line);
    snprintf(line, sizeof(line),
             "xbox:   eax %08lx ebx %08lx ecx %08lx edx %08lx esi %08lx edi %08lx "
             "ebp %08lx esp %08lx\n",
             (unsigned long)c->Eax, (unsigned long)c->Ebx, (unsigned long)c->Ecx,
             (unsigned long)c->Edx, (unsigned long)c->Esi, (unsigned long)c->Edi,
             (unsigned long)c->Ebp, (unsigned long)c->Esp);
    OutputDebugStringA(line);
    /* the stack can be the fault (an overflow); read it only when the
     * pointer is a plausible one */
    if (sp != NULL && ((uintptr_t)sp & 3) == 0 && !IsBadReadPtr(sp, 512 * 4))
    {
        n = snprintf(line, sizeof(line), "xbox:   stack:");
        for (i = 0; i < 512 && n < (int)sizeof(line) - 12; i++)
            if (sp[i] >= 0x00011000u && sp[i] < 0x01000000u)
                n += snprintf(line + n, sizeof(line) - n, " %08lx", (unsigned long)sp[i]);
        snprintf(line + n, sizeof(line) - n, "\n");
        OutputDebugStringA(line);
    }
    return 0;                   /* EXCEPTION_CONTINUE_SEARCH: as before */
}

/* The game leaves with exit(): its quit combination (START+A+B+X+Y,
 * src/sys/sys_joybus.c) and its "no data" path. On the Dreamcast that
 * returns to the loader; RXDK's exit ends in a spin, so the way out is
 * taken here, on the way through exit's handlers: the dashboard. */
static void xbox_exit_to_dashboard(void)
{
    arch_exit();
}

void __cdecl main(void)
{
    static char *argv[] = { "default.xbe", NULL };

    SetUnhandledExceptionFilter(xbox_fault_report);
    xbox_fpu_setup();
    atexit(xbox_exit_to_dashboard);
    /* the game ports enumerate asynchronously: start now, so the pads are
     * known by the time the game first looks */
    xbox_maple_init();
    /* the port's options (sf64x.ini in the save game): before the game, so
       the first level built already sees them */
    xbox_settings_load();
    {
        extern int xbox_pvr_tr_sorted;
        xbox_pvr_tr_sorted = gXboxCfg.trSorted;
    }

    /* The game loop does not return; main does only when it could not
     * start. Say so: a title that drops to the dashboard with no word is a
     * riddle. */
    dbglog(DBG_CRITICAL, "xbox: the game's main returned %d\n", sf64_main(1, argv));
    arch_exit();
}
