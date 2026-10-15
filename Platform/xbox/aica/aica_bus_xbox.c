/* aica_bus_xbox.c -- the SH-4's side of the AICA on the Xbox: sound RAM,
 * its allocator, the SH-4 -> ARM command queue, the SPU load/enable calls
 * and the G2 bus accessors. See aica_xbox.h for the model. */
#include "../xbox_xtl.h"

#include <stdlib.h>
#include <string.h>

#include <kos/dbglog.h>
#include <dc/sound/sound.h>
#include <dc/sound/aica_comm.h>
#include <dc/spu.h>
#include <dc/g2bus.h>
#include "../aica_cmd_iface.h"
#include "aica_xbox.h"

uint8_t *xbox_aica_ram;
volatile unsigned char xbox_aica_regs[0x8000];

static CRITICAL_SECTION sQueueLock;
static int sBusReady;

static void bus_init(void)
{
    if (sBusReady)
        return;
    /* contiguous, so sample data never straddles anything odd; and above
     * AICA_RAM_SIZE, which is what lets aica_host tell the two address
     * forms apart */
    xbox_aica_ram = (uint8_t *)VirtualAlloc(NULL, AICA_RAM_SIZE, MEM_RESERVE | MEM_COMMIT,
                                            PAGE_READWRITE);
    if (xbox_aica_ram == NULL || (uintptr_t)xbox_aica_ram < AICA_RAM_SIZE)
    {
        dbglog(DBG_CRITICAL, "aica: no sound RAM\n");
        return;
    }
    memset(xbox_aica_ram, 0, AICA_RAM_SIZE);
    InitializeCriticalSection(&sQueueLock);
    sBusReady = 1;
}

/* ============================================================ the SPU */

void spu_memload(uintptr_t to, const void *from, size_t length)
{
    uint8_t *d;

    bus_init();
    d = (uint8_t *)aica_host((uint32_t)to);
    /* KOS moves whole 32-bit words */
    memcpy(d, from, (length + 3) & ~(size_t)3);
}

void spu_memload_sq(uintptr_t to, const void *from, size_t length)
{
    uint8_t *d;

    bus_init();
    d = (uint8_t *)aica_host((uint32_t)to);
    /* the store-queue path moves whole 32-byte lines */
    memcpy(d, from, (length + 31) & ~(size_t)31);
}

void spu_memload_dma(uintptr_t to, const void *from, size_t length)
{
    spu_memload(to, from, length);
}

void spu_memread(void *to, uintptr_t from, size_t length)
{
    bus_init();
    memcpy(to, aica_host((uint32_t)from), length);
}

void spu_memset(uintptr_t to, uint32_t what, size_t length)
{
    uint32_t *d;
    size_t i;

    bus_init();
    d = (uint32_t *)aica_host((uint32_t)to);
    for (i = 0; i < length / 4; i++)
        d[i] = what;
}

void spu_memset_sq(uintptr_t to, uint32_t what, size_t length)
{
    spu_memset(to, what, length);
}

/* The ARM is held in reset until spu_enable lets it run: here that is when
 * the firmware is booted and the mixer thread that clocks it starts. */
void spu_disable(void)
{
    bus_init();
    aica_fw_running = 0;
}

void spu_enable(void)
{
    bus_init();
    if (!sBusReady)
        return;
    aica_fw_boot();
    aica_fw_running = 1;
    aica_mixer_start();
}

void spu_master_mixer(int volume, int stereo)
{
    (void)stereo;
    xbox_aica_regs[0x2800] = (unsigned char)(volume & 0x0F);
}

int spu_init(void)
{
    bus_init();
    return sBusReady ? 0 : -1;
}

int spu_shutdown(void)
{
    return 0;
}

void spu_reset_chans(void)
{
}

/* ============================================================ the G2 bus */

static volatile uint8_t *g2_at(uintptr_t a, int width)
{
    bus_init();
    if (a >= 0xa0800000u && a < 0xa0800000u + AICA_RAM_SIZE)
        return xbox_aica_ram + (a - 0xa0800000u);
    if (a >= 0xa0700000u && a < 0xa0700000u + sizeof(xbox_aica_regs))
        return xbox_aica_regs + (a - 0xa0700000u);
    if (xbox_aica_ram != NULL && a >= (uintptr_t)xbox_aica_ram &&
        a + width <= (uintptr_t)xbox_aica_ram + AICA_RAM_SIZE)
        return (volatile uint8_t *)a;
    {
        static int told;

        if (told++ < 8)
            dbglog(DBG_ERROR, "g2: access at %08lx is not the AICA\n", (unsigned long)a);
    }
    return NULL;
}

uint8_t g2_read_8(uintptr_t address)
{
    volatile uint8_t *p = g2_at(address, 1);

    return p ? *p : 0;
}

void g2_write_8(uintptr_t address, uint8_t value)
{
    volatile uint8_t *p = g2_at(address, 1);

    if (p)
        *p = value;
}

uint16_t g2_read_16(uintptr_t address)
{
    volatile uint16_t *p = (volatile uint16_t *)g2_at(address, 2);

    return p ? *p : 0;
}

void g2_write_16(uintptr_t address, uint16_t value)
{
    volatile uint16_t *p = (volatile uint16_t *)g2_at(address, 2);

    if (p)
        *p = value;
}

uint32_t g2_read_32(uintptr_t address)
{
    volatile uint32_t *p = (volatile uint32_t *)g2_at(address, 4);

    return p ? *p : 0;
}

void g2_write_32(uintptr_t address, uint32_t value)
{
    volatile uint32_t *p = (volatile uint32_t *)g2_at(address, 4);

    if (p)
        *p = value;
}

void g2_read_block_32(uint32_t *output, uintptr_t address, size_t amt)
{
    size_t i;

    for (i = 0; i < amt; i++)
        output[i] = g2_read_32(address + 4 * i);
}

void g2_write_block_32(const uint32_t *input, uintptr_t address, size_t amt)
{
    size_t i;

    for (i = 0; i < amt; i++)
        g2_write_32(address + 4 * i, input[i]);
}

void g2_fifo_wait(void)
{
}

/* ====================================================== sound memory
 * KOS's snd_mem (kernel/arch/dreamcast/sound/snd_mem.c): a first-fit
 * allocator over sound RAM above `reserve`, 32-byte blocks. Here the
 * blocks are handed out as host addresses (see aica_xbox.h). */

typedef struct SndBlock
{
    uint32_t off, size;
    int used;
} SndBlock;

static SndBlock *sBlocks;
static uint32_t sBlockCount, sBlockMax;
static int sMemReady;
static CRITICAL_SECTION sMemLock;

int snd_mem_init(uint32_t reserve)
{
    bus_init();
    if (!sBusReady)
        return -1;
    if (!sMemReady)
        InitializeCriticalSection(&sMemLock);
    EnterCriticalSection(&sMemLock);
    free(sBlocks);
    sBlockMax = 256;
    sBlocks = (SndBlock *)malloc(sBlockMax * sizeof(SndBlock));
    reserve = (reserve + 31) & ~31u;
    sBlocks[0].off = reserve;
    sBlocks[0].size = AICA_RAM_SIZE - reserve;
    sBlocks[0].used = 0;
    sBlockCount = 1;
    sMemReady = 1;
    LeaveCriticalSection(&sMemLock);
    return 0;
}

void snd_mem_shutdown(void)
{
}

uint32_t snd_mem_malloc(size_t size)
{
    uint32_t i, want, rv = 0;

    if (!sMemReady || size == 0)
        return 0;
    want = (uint32_t)((size + 31) & ~(size_t)31);
    EnterCriticalSection(&sMemLock);
    for (i = 0; i < sBlockCount; i++)
    {
        SndBlock *b = &sBlocks[i];

        if (b->used || b->size < want)
            continue;
        if (b->size > want)
        {
            if (sBlockCount == sBlockMax)
            {
                SndBlock *nb = (SndBlock *)realloc(sBlocks, 2 * sBlockMax * sizeof(SndBlock));

                if (nb == NULL)
                    break;
                sBlocks = nb;
                sBlockMax *= 2;
                b = &sBlocks[i];
            }
            memmove(&sBlocks[i + 2], &sBlocks[i + 1], (sBlockCount - i - 1) * sizeof(SndBlock));
            sBlocks[i + 1].off = b->off + want;
            sBlocks[i + 1].size = b->size - want;
            sBlocks[i + 1].used = 0;
            sBlockCount++;
            b->size = want;
        }
        b->used = 1;
        rv = (uint32_t)(uintptr_t)(xbox_aica_ram + b->off);
        break;
    }
    LeaveCriticalSection(&sMemLock);
    return rv;
}

void snd_mem_free(uint32_t addr)
{
    uint32_t off, i;

    if (!sMemReady || addr == 0)
        return;
    off = (uint32_t)((uint8_t *)aica_host(addr) - xbox_aica_ram);
    EnterCriticalSection(&sMemLock);
    for (i = 0; i < sBlockCount; i++)
    {
        if (sBlocks[i].off == off && sBlocks[i].used)
        {
            sBlocks[i].used = 0;
            if (i + 1 < sBlockCount && !sBlocks[i + 1].used)
            {
                sBlocks[i].size += sBlocks[i + 1].size;
                memmove(&sBlocks[i + 1], &sBlocks[i + 2], (sBlockCount - i - 2) * sizeof(SndBlock));
                sBlockCount--;
            }
            if (i > 0 && !sBlocks[i - 1].used)
            {
                sBlocks[i - 1].size += sBlocks[i].size;
                memmove(&sBlocks[i], &sBlocks[i + 1], (sBlockCount - i - 1) * sizeof(SndBlock));
                sBlockCount--;
            }
            break;
        }
    }
    LeaveCriticalSection(&sMemLock);
}

/* KOS: the largest free block */
uint32_t snd_mem_available(void)
{
    uint32_t i, largest = 0;

    if (!sMemReady)
        return 0;
    EnterCriticalSection(&sMemLock);
    for (i = 0; i < sBlockCount; i++)
        if (!sBlocks[i].used && sBlocks[i].size > largest)
            largest = sBlocks[i].size;
    LeaveCriticalSection(&sMemLock);
    return largest;
}

/* ===================================================== the command queue
 * KOS's snd_sh4_to_aica (snd_iface.c): copy the packet into the queue in
 * sound RAM at the head, wrapping, then publish the new head. The firmware
 * (aica_fw_xbox.c) drains from the tail on its next pass. */

int snd_sh4_to_aica(void *packet, uint32_t size)
{
    volatile aica_queue_t *q;
    uint32_t bot, top, start;
    const uint32_t *pkt32 = (const uint32_t *)packet;

    bus_init();
    if (!sBusReady)
        return -1;
    q = (volatile aica_queue_t *)(xbox_aica_ram + AICA_MEM_CMD_QUEUE);
    if (!q->valid)
    {
        static int told;

        if (told++ < 4)
            dbglog(DBG_ERROR, "snd_sh4_to_aica: queue is not yet valid\n");
        return -1;
    }
    EnterCriticalSection(&sQueueLock);
    bot = q->data;
    top = bot + q->size;
    start = bot + q->head;
    while (size-- > 0)
    {
        *(volatile uint32_t *)aica_host(start) = *pkt32++;
        start += 4;
        if (start >= top)
            start = bot;
    }
    /* the packet before the head that publishes it: x86 keeps stores in
     * order, the compiler must too */
    __asm__ volatile("" ::: "memory");
    q->head = start - bot;
    LeaveCriticalSection(&sQueueLock);
    return 0;
}

void snd_sh4_to_aica_start(void)
{
    volatile aica_queue_t *q = (volatile aica_queue_t *)(xbox_aica_ram + AICA_MEM_CMD_QUEUE);

    q->process_ok = 1;
}

void snd_sh4_to_aica_stop(void)
{
    volatile aica_queue_t *q = (volatile aica_queue_t *)(xbox_aica_ram + AICA_MEM_CMD_QUEUE);

    q->process_ok = 0;
}

/* KOS 2.2's snd_init (snd_iface.c): hold the ARM, clear the driver's corner
 * of sound RAM, load the stream driver, let the ARM run, and hand the rest
 * of sound RAM (from AICA_RAM_START) to the allocator. The driver here is
 * KOS's stock firmware compiled for the x86 (aica_fw_xbox.c), which
 * spu_enable boots; there is no image to copy. */
static int sSndInitted;

int snd_init(void)
{
    if (sSndInitted)
        return 0;
    if (spu_init() < 0)
        return -1;
    spu_disable();
    spu_memset_sq(0, 0, AICA_RAM_START);
    spu_enable();
    if (!aica_fw_running)
        return -1;
    if (snd_mem_init(AICA_RAM_START) < 0)
        return -1;
    sSndInitted = 1;
    return 0;
}

void snd_shutdown(void)
{
    if (!sSndInitted)
        return;
    spu_disable();
    snd_mem_shutdown();
    sSndInitted = 0;
}

uint16_t snd_get_pos(unsigned int ch)
{
    volatile aica_channel_t *c;

    if (ch >= 64 || !sBusReady)
        return 0;
    c = (volatile aica_channel_t *)(xbox_aica_ram + AICA_CHANNEL(ch));
    return (uint16_t)(c->pos & 0xFFFF);
}

bool snd_is_playing(unsigned int ch)
{
    if (ch >= 64)
        return false;
    return (xbox_aica_regs[0x80 * ch + 1] & 0x40) != 0;
}
