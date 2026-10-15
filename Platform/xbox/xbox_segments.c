/* xbox_segments.c -- the N64 segments at their own addresses.
 *
 * The Dreamcast port links every asset at its N64 segment address (the
 * Corneria data at 0x06000000, the common data at 0x01000000, ...) and
 * makes those addresses real with the SH-4's MMU: src/sys/sys_main.c's
 * segment_init maps 64 KB pages at 0x0S000000 onto each segment's buffer
 * (src/dmatable.c SEG_BUF). So SEGMENTED_TO_VIRTUAL is the identity, and
 * the game reads its asset data straight through the asset symbols.
 *
 * The Xbox does the same with its own memory manager: the buffers ARE at
 * those addresses. Before anything else can take that address range, a
 * startup constructor reserves 0x01000000-0x0FFFFFFF (address space only);
 * segment_init then commits, at 0x0S000000, room for the largest file that
 * segment is ever loaded with (tools/xbox/build_assets.py measures them,
 * gen/seg_sizes.h) and points SEG_BUF there. Segment B is never loaded
 * (its scene entries are all empty) and nothing is linked at 0x0B000000;
 * as on the Dreamcast, its SEG_BUF entry is segment 6's buffer. */
#include "xbox_xtl.h"

#include <stdint.h>

#include <kos/dbglog.h>
#include <arch/arch.h>

#include "gen/seg_sizes.h"

extern uint8_t *SEG_BUF[15];
extern uint32_t SEG_PAGECOUNT[15];

#define SEG_SPAN    0x01000000u     /* one segment's address range */
#define SEG_PAGE    0x00010000u     /* the Dreamcast's TLB page size */
#define SEG_FIRST   0x01000000u
#define SEG_END     0x10000000u

static void *sReserved;

/* As early as the C runtime allows: before main, before the XAPI heap has
 * grown, before Direct3D. */
__attribute__((constructor(101))) static void xbox_segments_reserve(void)
{
    sReserved = VirtualAlloc((void *)(uintptr_t)SEG_FIRST, SEG_END - SEG_FIRST, MEM_RESERVE,
                             PAGE_READWRITE);
}

void xbox_segments_init(void)
{
    uint32_t s;

    if (sReserved != (void *)(uintptr_t)SEG_FIRST)
    {
        dbglog(DBG_CRITICAL, "segments: 0x%08x-0x%08x is not free (got %p); the asset "
               "addresses cannot be made real\n", SEG_FIRST, SEG_END, sReserved);
        arch_abort();
    }
    for (s = 1; s < 16; s++)
    {
        uint32_t base = s * SEG_SPAN, size;
        void *p;

        if (s == 0xB)
            continue;
        /* the largest file, plus the 2 KB the Dreamcast's buffers carry
         * past it (src/dmatable.c), in whole 64 KB pages */
        size = (xbox_seg_file_max[s] + 2048u + SEG_PAGE - 1) & ~(SEG_PAGE - 1);
        p = VirtualAlloc((void *)(uintptr_t)base, size, MEM_COMMIT, PAGE_READWRITE);
        if (p != (void *)(uintptr_t)base)
        {
            dbglog(DBG_CRITICAL, "segments: cannot commit %u KB at 0x%08x\n",
                   (unsigned)(size / 1024), (unsigned)base);
            arch_abort();
        }
        SEG_BUF[s - 1] = (uint8_t *)p;
        SEG_PAGECOUNT[s - 1] = size / SEG_PAGE;
    }
    SEG_BUF[0xB - 1] = SEG_BUF[6 - 1];
    SEG_PAGECOUNT[0xB - 1] = SEG_PAGECOUNT[6 - 1];
    dbglog(DBG_INFO, "segments: 1-F committed at their N64 addresses\n");
}
