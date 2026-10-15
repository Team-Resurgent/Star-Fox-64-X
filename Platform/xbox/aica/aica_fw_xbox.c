/* aica_fw_xbox.c -- KallistiOS's stock AICA firmware, on the x86.
 *
 * The Dreamcast port drives the sound chip with KOS's own ARM7 driver
 * (stream.drv): its voice driver (src/audio/aica_synth.c) queues
 * AICA_CMD_CHAN packets -- start, stop, update freq/vol/pan -- and the
 * firmware plays them on the chip's channels. This is that firmware:
 *
 *   KallistiOS kernel/arch/dreamcast/sound/arm/main.c -- the command queue
 *     loop (process_cmd_queue, process_one, process_chn)
 *   KallistiOS kernel/arch/dreamcast/sound/arm/aica.c -- the channel
 *     routines, which write the channel registers exactly as KOS does
 *   ((c)2000-2002 Megan Potter, (c)2024 Stefanos Kornilios Mitsis Poiitidis,
 *    (c)2026 Ruslan Rostovtsev; KOS licence, Platform/xbox/LICENSE.KOS)
 *
 * re-expressed for a machine on which sound RAM is not at address 0: the
 * queue, the channel mirror and the clock word go through aica_host(). The
 * loop is not a loop: the mixer (aica_mixer_xbox.c) runs one pass every 100
 * output samples, which is what the firmware's timer_wait(10) between
 * passes measures out at the clock's 4410 Hz.
 *
 * Key on and key off, which on the chip take effect when a register write
 * sets KYONEX, are handed to the mixer at that write. The one register value
 * that cannot be kept as KOS writes it is the sample address: 23 bits of it
 * are packed into two registers, and a host address (snd_mem_malloc hands
 * those out, see aica_xbox.h) does not fit, so the whole address is kept
 * beside them (xbox_aica_chan_base). */
#include <stddef.h>
#include <string.h>

#include "../aica_cmd_iface.h"
#include "../aica.h"
#include "aica_xbox.h"

int aica_fw_running;
uint32_t xbox_aica_chan_base[64];

#define CLOCK (*(volatile uint32 *)aica_host(AICA_MEM_CLOCK))

static volatile aica_queue_t *q_cmd;
static volatile aica_queue_t *q_resp;
volatile aica_channel_t *chans;

/* ============================================================ aica.c */

void aica_init(void)
{
    int i, j;

    /* Initialize AICA channels */
    SNDREG32(0x2800) = 0x0000;

    for (i = 0; i < 64; i++)
    {
        aica_mixer_key_off(i);
        CHNREG32(i, 0) = 0x8000;
        for (j = 4; j < 0x80; j += 4)
            CHNREG32(i, j) = 0;
        CHNREG32(i, 20) = 0x1f;
        xbox_aica_chan_base[i] = 0;
    }

    SNDREG32(0x2800) = 0x000f;
}

/* 16 * log2(255 / i), as KOS tabulates it */
static const uint8 logs[] = {
    255, 127, 111, 102, 95, 90, 86, 82, 79, 77, 74, 72, 70, 68, 66, 65,
    63, 62, 61, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 50, 49, 48,
    47, 47, 46, 45, 45, 44, 43, 43, 42, 42, 41, 41, 40, 40, 39, 39,
    38, 38, 37, 37, 36, 36, 35, 35, 34, 34, 34, 33, 33, 33, 32, 32,
    31, 31, 31, 30, 30, 30, 29, 29, 29, 28, 28, 28, 27, 27, 27, 27,
    26, 26, 26, 25, 25, 25, 25, 24, 24, 24, 24, 23, 23, 23, 23, 22,
    22, 22, 22, 21, 21, 21, 21, 20, 20, 20, 20, 20, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17, 16, 16, 16, 16, 16,
    15, 15, 15, 15, 15, 15, 14, 14, 14, 14, 14, 14, 13, 13, 13, 13,
    13, 13, 12, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11, 11, 10,
    10, 10, 10, 10, 10, 10, 9, 9, 9, 9, 9, 9, 9, 8, 8, 8,
    8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6,
    6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static inline uint8 calc_aica_vol(uint8 x)
{
    return logs[x];
}

static inline int calc_aica_pan(int x)
{
    if (x == 0x80)
        return 0;
    else if (x < 0x80)
        return 0x10 | ((0x7f - x) >> 3);
    else
        return (x - 0x80) >> 3;
}

static void freq_regs(int ch, uint32 freq)
{
    uint32 freq_lo, freq_base = 5644800;
    int freq_hi = 7;

    /* Formula is freq = 44100*2^freq_hi*(1+freq_lo/1024) */
    while (freq < freq_base && freq_hi > -8)
    {
        freq_base >>= 1;
        --freq_hi;
    }
    freq_lo = (freq << 10) / freq_base;
    CHNREG32(ch, 24) = (freq_hi << 11) | (freq_lo & 1023);
}

/* A KYONEX write: every channel whose KYONB is set and is not sounding
 * starts, every one whose KYONB is clear and is sounding stops -- the
 * chip's rule, which aica_play, aica_stop and aica_sync_play rely on. */
static void key_execute(void)
{
    int i;

    for (i = 0; i < 64; i++)
    {
        uint32 r = CHNREG32(i, 0);

        if (r & AICA_CHANNEL_KEYONB)
            aica_mixer_key_on(i);
        else
            aica_mixer_key_off(i);
    }
}

void aica_play(int ch, int delay)
{
    uint32 smpptr = chans[ch].base;
    uint32 mode = chans[ch].type;
    uint32 loopst = chans[ch].loopstart;
    uint32 loopend = chans[ch].loopend;
    uint32 freq = chans[ch].freq;
    uint32 vol = chans[ch].vol;
    uint32 pan = chans[ch].pan;
    uint32 loopflag = chans[ch].loop;
    uint32 playCont;

    /* Stop the channel (if it's already playing) */
    aica_stop(ch);

    CHNREG32(ch, 8) = loopst & 0xffff;
    CHNREG32(ch, 12) = loopend & 0xffff;
    freq_regs(ch, freq);
    CHNREG8(ch, 36) = calc_aica_pan(pan);
    CHNREG8(ch, 37) = 0xf;
    /* turn off Low Pass Filter (LPF) */
    CHNREG8(ch, 40) = 0x24;
    CHNREG8(ch, 41) = calc_aica_vol(vol);
    /* No volume envelope */
    CHNREG32(ch, 16) = 0x1f;

    CHNREG32(ch, 4) = smpptr & 0xffff;
    playCont = (mode << 7) | ((smpptr >> 16) & 0x7f);
    if (loopflag)
        playCont |= 0x0200;
    xbox_aica_chan_base[ch] = smpptr;

    if (delay)
    {
        CHNREG32(ch, 0) = playCont;             /* key off */
    }
    else
    {
        CHNREG32(ch, 0) = 0xc000 | playCont;    /* key on */
        key_execute();
    }
}

/* Start sound on all channels specified by chmap bitmap */
void aica_sync_play(uint32 chmap)
{
    int i = 0;

    while (chmap)
    {
        if (chmap & 0x1)
            CHNREG32(i, 0) = CHNREG32(i, 0) | 0xc000;
        i++;
        chmap >>= 1;
    }
    key_execute();
}

void aica_stop(int ch)
{
    CHNREG32(ch, 0) = (CHNREG32(ch, 0) & ~0x4000) | 0x8000;
    key_execute();
}

void aica_vol(int ch)
{
    CHNREG8(ch, 41) = calc_aica_vol((uint8)chans[ch].vol);
}

void aica_pan(int ch)
{
    CHNREG8(ch, 36) = calc_aica_pan(chans[ch].pan);
}

void aica_freq(int ch)
{
    freq_regs(ch, chans[ch].freq);
}

int aica_get_pos(int ch)
{
    chans[ch].pos = aica_mixer_pos(ch) & 0xffff;
    return chans[ch].pos;
}

/* ============================================================ main.c */

static void process_chn(uint32 chn, const aica_channel_t *chndat)
{
    switch (chndat->cmd & AICA_CH_CMD_MASK)
    {
        case AICA_CH_CMD_NONE:
            break;
        case AICA_CH_CMD_START:
            if (chndat->cmd & AICA_CH_START_SYNC)
            {
                /* the channel map: cmd_id is a 64-bit mask here, of which
                 * the 32-bit packet carries the low word (as KOS's does) */
                aica_sync_play(chn);
            }
            else if (chn < 64)
            {
                memcpy((void *)(chans + chn), chndat, sizeof(aica_channel_t));
                chans[chn].pos = 0;
                aica_play((int)chn, chndat->cmd & AICA_CH_START_DELAY);
            }
            break;
        case AICA_CH_CMD_STOP:
            if (chn < 64)
                aica_stop((int)chn);
            break;
        case AICA_CH_CMD_UPDATE:
            if (chn >= 64)
                break;
            if (chndat->cmd & AICA_CH_UPDATE_SET_FREQ)
            {
                chans[chn].freq = chndat->freq;
                aica_freq((int)chn);
            }
            if (chndat->cmd & AICA_CH_UPDATE_SET_VOL)
            {
                chans[chn].vol = chndat->vol;
                aica_vol((int)chn);
            }
            if (chndat->cmd & AICA_CH_UPDATE_SET_PAN)
            {
                chans[chn].pan = chndat->pan;
                aica_pan((int)chn);
            }
            break;
        default:
            /* error */
            break;
    }
}

/* Process one packet of queue data; returns its size in words. A size of
 * 0 would leave the ARM spinning on the same packet for ever; it is taken
 * as one word so a corrupt queue cannot hang the mixer thread. */
static uint32 process_one(uint32 tail)
{
    uint32 pktdata[AICA_CMD_MAX_SIZE];
    const aica_cmd_t *pkt = (const aica_cmd_t *)pktdata;
    uint32 base = q_cmd->data, qsize = q_cmd->size;
    uint32 size, i;

    size = *(volatile uint32 *)aica_host(base + tail);
    if (size == 0)
        size = 1;
    if (size > AICA_CMD_MAX_SIZE)
        size = AICA_CMD_MAX_SIZE;
    for (i = 0; i < size; i++)
    {
        pktdata[i] = *(volatile uint32 *)aica_host(base + tail);
        tail += 4;
        if (tail >= qsize)
            tail = 0;
    }

    switch (pkt->cmd)
    {
        case AICA_CMD_NONE:
            break;
        case AICA_CMD_PING:
            /* Not implemented yet (KOS) */
            break;
        case AICA_CMD_CHAN:
            process_chn(pkt->cmd_id, (const aica_channel_t *)pkt->cmd_data);
            break;
        case AICA_CMD_SYNC_CLOCK:
            /* Reset our timer clock to zero */
            CLOCK = 0;
            break;
        default:
            /* error */
            break;
    }
    return size;
}

static void process_cmd_queue(void)
{
    uint32 head, tail, tsloc, ts, size;

    /* Grab these values up front in case SH-4 changes head */
    head = q_cmd->head;
    tail = q_cmd->tail;
    size = q_cmd->size;

    while (head != tail)
    {
        /* Look at the next packet. If our clock isn't there yet, then we
         * won't process anything yet either. */
        tsloc = tail + offsetof(aica_cmd_t, timestamp);
        if (tsloc >= size)
            tsloc -= size;
        ts = *(volatile uint32 *)aica_host(q_cmd->data + tsloc);
        if (ts > 0 && ts >= CLOCK)
            return;

        ts = process_one(tail);
        tail += ts * 4;
        if (tail >= size)
            tail -= size;
        q_cmd->tail = tail;
    }
}

static void queue_init(volatile aica_queue_t *q, uint32 base, uint32 end)
{
    q->head = 0;
    q->tail = 0;
    q->data = base + sizeof(aica_queue_t);
    q->size = end - q->data;
    q->process_ok = 1;
    q->valid = 1;
}

/* arm_main up to its loop. Run by spu_enable, before the mixer starts. */
void aica_fw_boot(void)
{
    q_cmd = (volatile aica_queue_t *)aica_host(AICA_MEM_CMD_QUEUE);
    q_resp = (volatile aica_queue_t *)aica_host(AICA_MEM_RESP_QUEUE);
    chans = (volatile aica_channel_t *)aica_host(AICA_MEM_CHANNELS);

    queue_init(q_cmd, AICA_MEM_CMD_QUEUE, AICA_MEM_RESP_QUEUE);
    queue_init(q_resp, AICA_MEM_RESP_QUEUE, AICA_MEM_CHANNELS);
    aica_init();
    CLOCK = 0;
}

/* One pass of arm_main's loop: update the channel position counters, then
 * take the commands that are due. The mixer advances CLOCK by 10 before
 * each call. */
void aica_fw_pass(void)
{
    int i;

    if (!aica_fw_running)
        return;
    for (i = 0; i < 64; i++)
        aica_get_pos(i);
    if (q_cmd->process_ok)
        process_cmd_queue();
}
