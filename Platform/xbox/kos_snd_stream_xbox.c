/* kos_snd_stream_xbox.c -- KallistiOS's sound streams on the emulated AICA.
 *
 * KallistiOS v2.2.1 (the version the Dreamcast port is built against)
 * kernel/arch/dreamcast/sound/snd_stream.c ((c)2000-2004 Megan Potter,
 * (c)2002 Florian Schulze, (c)2020 Lawrence Sebald, (c)2023-2025 Ruslan
 * Rostovtsev, (c)2024 Stefanos Kornilios Mitsis Poiitidis; KOS licence,
 * Platform/xbox/LICENSE.KOS), with the channel reservation of snd_sfxmgr.c
 * and the splitters of snd_pcm_split.s.
 *
 * A stream is a ring buffer per channel in sound RAM, played in a loop by
 * one AICA channel each (two for stereo, started together); snd_stream_poll
 * reads where the channel is, and refills the half behind it from the
 * user's callback. Here the transfers into sound RAM are plain copies (the
 * Dreamcast's SPU DMA and store queues move the same bytes to the same
 * place), and the channel position is the one the firmware mirrors from
 * the mixer (aica_fw_xbox.c aica_get_pos). Everything else is KOS's, step
 * for step.
 *
 * In this port the user is src/audio/sndwav.c: it initialises the stream
 * system and services streams it would have opened for WAV music. The game
 * opens none (its music is sequenced, src/audio/aica_synth.c), so at run
 * time only snd_stream_init is reached; the rest is here so that sndwav.c
 * means on the Xbox what it means on the Dreamcast. */
#include "xbox_xtl.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <kos/dbglog.h>
#include <kos/sem.h>
#include <kos/thread.h>
#include <dc/spu.h>
#include <dc/sound/sound.h>
#include <dc/sound/stream.h>
#include <dc/sound/sfxmgr.h>
#include <dc/sound/aica_comm.h>

#include "aica_cmd_iface.h"
#include "aica/aica_xbox.h"

/* ========================================== channel reservation (sfxmgr) */

static uint64_t sfx_inuse;
static CRITICAL_SECTION sChnLock;
static volatile LONG sChnLockState;

static void chn_lock(void)
{
    if (sChnLockState != 2)
    {
        if (InterlockedCompareExchange((LONG *)&sChnLockState, 1, 0) == 0)
        {
            InitializeCriticalSection(&sChnLock);
            sChnLockState = 2;
        }
        while (sChnLockState != 2)
            Sleep(1);
    }
    EnterCriticalSection(&sChnLock);
}

int snd_sfx_chn_alloc(void)
{
    int chn;

    chn_lock();
    for (chn = 0; chn < 64; chn++)
        if (!(sfx_inuse & (1ULL << chn)))
            break;
    if (chn >= 64)
        chn = -1;
    else
        sfx_inuse |= 1ULL << chn;
    LeaveCriticalSection(&sChnLock);
    return chn;
}

void snd_sfx_chn_free(int chn)
{
    if (chn < 0 || chn >= 64)
        return;
    chn_lock();
    sfx_inuse &= ~(1ULL << chn);
    LeaveCriticalSection(&sChnLock);
}

/* ======================================================= the splitters */

/* interleaved 16-bit pairs -> two mono runs; size in bytes of the source.
 * v2.2.1's splitter stores each pair's second sample to `left` and its
 * first to `right` (snd_pcm_split.s: r11 holds the high halfwords, r12 the
 * low; later KOS swaps the two stores). */
void snd_pcm16_split(uint32_t *data, uint32_t *left, uint32_t *right, size_t size)
{
    const uint16_t *s = (const uint16_t *)data;
    uint16_t *l = (uint16_t *)left, *r = (uint16_t *)right;
    size_t i, n = size / 4;

    for (i = 0; i < n; i++)
    {
        l[i] = s[2 * i + 1];
        r[i] = s[2 * i];
    }
}

/* interleaved 8-bit L,R -> two mono runs */
void snd_pcm8_split(uint32_t *data, uint32_t *left, uint32_t *right, size_t size)
{
    const uint8_t *s = (const uint8_t *)data;
    uint8_t *l = (uint8_t *)left, *r = (uint8_t *)right;
    size_t i, n = size / 2;

    for (i = 0; i < n; i++)
    {
        l[i] = s[2 * i];
        r[i] = s[2 * i + 1];
    }
}

/* interleaved 4-bit: of each source byte the low nibble is the right
 * channel's sample and the high nibble the left's (snd_pcm_split.s:
 * dst_r = src & 0xf, dst_l = src >> 4 & 0xf, packed low nibble first) */
void snd_adpcm_split(uint32_t *data, uint32_t *left, uint32_t *right, size_t size)
{
    const uint8_t *s = (const uint8_t *)data;
    uint8_t *l = (uint8_t *)left, *r = (uint8_t *)right;
    size_t i, n = size / 2;

    for (i = 0; i < n; i++)
    {
        uint8_t a = s[2 * i], b = s[2 * i + 1];

        r[i] = (uint8_t)((a & 0x0F) | ((b & 0x0F) << 4));
        l[i] = (uint8_t)(((a >> 4) & 0x0F) | (b & 0xF0));
    }
}

/* ============================================================ streams */

typedef struct filter
{
    struct filter *next;
    snd_stream_filter_t func;
    void *data;
} filter_t;

typedef struct strchan
{
    int ch[2];
    uint32_t last_write_pos;
    size_t buffer_size;
    uint32_t spu_ram_sch[2];
    snd_stream_callback_t get_data;
    snd_stream_callback_direct_t req_data;
    filter_t *filters;
    int type;
    int bitsize;
    int channels;
    int frequency;
    int queueing;
    volatile int initted;
    void *user_data;
} strchan_t;

static strchan_t streams[SND_STREAM_MAX];
static uint32_t *sep_buffer[2];
static semaphore_t stream_sem;
static int max_channels;
static size_t max_buffer_size;

static int check_hnd(snd_stream_hnd_t hnd)
{
    if (hnd < 0 || hnd >= SND_STREAM_MAX || !streams[hnd].initted)
    {
        dbglog(DBG_ERROR, "snd_stream: invalid handle %d\n", hnd);
        return 0;
    }
    return 1;
}

static size_t samples_to_bytes(snd_stream_hnd_t hnd, size_t samples)
{
    switch (streams[hnd].bitsize)
    {
        case 4: return samples >> 1;
        case 8: return samples;
        default: return samples << 1;
    }
}

static size_t bytes_to_samples(snd_stream_hnd_t hnd, size_t bytes)
{
    switch (streams[hnd].bitsize)
    {
        case 4: return bytes << 1;
        case 8: return bytes;
        default: return bytes >> 1;
    }
}

void snd_stream_set_callback(snd_stream_hnd_t hnd, snd_stream_callback_t cb)
{
    if (check_hnd(hnd))
        streams[hnd].get_data = cb;
}

void snd_stream_set_callback_direct(snd_stream_hnd_t hnd, snd_stream_callback_direct_t cb)
{
    if (check_hnd(hnd))
        streams[hnd].req_data = cb;
}

void snd_stream_set_userdata(snd_stream_hnd_t hnd, void *d)
{
    if (check_hnd(hnd))
        streams[hnd].user_data = d;
}

void *snd_stream_get_userdata(snd_stream_hnd_t hnd)
{
    return check_hnd(hnd) ? streams[hnd].user_data : NULL;
}

void snd_stream_filter_add(snd_stream_hnd_t hnd, snd_stream_filter_t filtfunc, void *obj)
{
    filter_t *f, **pp;

    if (!check_hnd(hnd))
        return;
    f = (filter_t *)malloc(sizeof(*f));
    if (f == NULL)
        return;
    f->func = filtfunc;
    f->data = obj;
    f->next = NULL;
    for (pp = &streams[hnd].filters; *pp != NULL; pp = &(*pp)->next)
        ;
    *pp = f;
}

void snd_stream_filter_remove(snd_stream_hnd_t hnd, snd_stream_filter_t filtfunc, void *obj)
{
    filter_t **pp;

    if (!check_hnd(hnd))
        return;
    for (pp = &streams[hnd].filters; *pp != NULL; pp = &(*pp)->next)
    {
        if ((*pp)->func == filtfunc && (*pp)->data == obj)
        {
            filter_t *f = *pp;

            *pp = f->next;
            free(f);
            return;
        }
    }
}

static void process_filters(snd_stream_hnd_t hnd, void **buffer, int *samplecnt)
{
    filter_t *f;

    for (f = streams[hnd].filters; f != NULL; f = f->next)
        f->func(hnd, f->data, streams[hnd].frequency, streams[hnd].channels, buffer, samplecnt);
}

int snd_stream_init(void)
{
    return snd_stream_init_ex(2, SND_STREAM_BUFFER_MAX);
}

int snd_stream_init_ex(int channels, size_t buffer_size)
{
    if (max_channels)
    {
        if (channels > max_channels)
        {
            dbglog(DBG_ERROR, "snd_stream_init_ex(): already initialized with %d channels, "
                   "but %d requested\n", max_channels, channels);
            return -1;
        }
        else if (max_buffer_size && buffer_size > max_buffer_size)
        {
            dbglog(DBG_ERROR, "snd_stream_init_ex(): already initialized with %u buffer "
                   "size, but %u requested\n", (unsigned)max_buffer_size, (unsigned)buffer_size);
            return -1;
        }
        return 0;
    }

    if (sem_init(&stream_sem, 1) < 0)
        return -1;
    max_channels = channels;
    max_buffer_size = buffer_size;

    if (buffer_size > 0)
    {
        sep_buffer[0] = (uint32_t *)malloc(buffer_size);
        if (sep_buffer[0] == NULL)
        {
            dbglog(DBG_ERROR, "snd_stream_init_ex(): memory allocation failed\n");
            return -1;
        }
        sep_buffer[1] = sep_buffer[0] + (buffer_size / 8);
    }
    else
    {
        sep_buffer[0] = NULL;
        sep_buffer[1] = NULL;
    }

    if (snd_init() < 0)
    {
        dbglog(DBG_ERROR, "snd_stream_init_ex(): snd_init() failed, giving up\n");
        return -1;
    }
    return 0;
}

snd_stream_hnd_t snd_stream_alloc(snd_stream_callback_t cb, int bufsize)
{
    int i;
    snd_stream_hnd_t hnd = -1;

    for (i = 0; i < SND_STREAM_MAX; i++)
    {
        if (!streams[i].initted)
        {
            hnd = i;
            break;
        }
    }
    if (hnd == -1)
        return SND_STREAM_INVALID;

    streams[hnd].initted = 1;
    streams[hnd].buffer_size = (size_t)bufsize;
    streams[hnd].queueing = 0;
    snd_stream_set_callback(hnd, cb);
    snd_stream_set_callback_direct(hnd, NULL);
    streams[hnd].filters = NULL;

    streams[hnd].spu_ram_sch[0] = snd_mem_malloc(streams[hnd].buffer_size * max_channels);
    streams[hnd].ch[0] = snd_sfx_chn_alloc();
    if (max_channels == 2)
    {
        streams[hnd].spu_ram_sch[1] = streams[hnd].spu_ram_sch[0] + (uint32_t)streams[hnd].buffer_size;
        streams[hnd].ch[1] = snd_sfx_chn_alloc();
    }
    return hnd;
}

snd_stream_hnd_t snd_stream_reinit(snd_stream_hnd_t hnd, snd_stream_callback_t cb)
{
    if (!check_hnd(hnd))
        return SND_STREAM_INVALID;
    streams[hnd].queueing = 0;
    snd_stream_set_callback(hnd, cb);
    snd_stream_set_callback_direct(hnd, NULL);
    return hnd;
}

void snd_stream_destroy(snd_stream_hnd_t hnd)
{
    filter_t *c, *n;

    if (hnd < 0 || hnd >= SND_STREAM_MAX || !streams[hnd].initted)
        return;

    sem_wait(&stream_sem);
    snd_stream_stop(hnd);
    snd_sfx_chn_free(streams[hnd].ch[0]);
    if (max_channels == 2)
        snd_sfx_chn_free(streams[hnd].ch[1]);
    for (c = streams[hnd].filters; c != NULL; c = n)
    {
        n = c->next;
        free(c);
    }
    snd_mem_free(streams[hnd].spu_ram_sch[0]);
    memset(streams + hnd, 0, sizeof(streams[0]));
    sem_signal(&stream_sem);
}

void snd_stream_shutdown(void)
{
    int i;

    for (i = 0; i < SND_STREAM_MAX; i++)
        if (streams[i].initted)
            snd_stream_destroy(i);
    free(sep_buffer[0]);
    sep_buffer[0] = NULL;
    sep_buffer[1] = NULL;
    max_channels = 0;
    max_buffer_size = 0;
}

void snd_stream_queue_enable(snd_stream_hnd_t hnd)
{
    if (check_hnd(hnd))
        streams[hnd].queueing = 1;
}

void snd_stream_queue_disable(snd_stream_hnd_t hnd)
{
    if (check_hnd(hnd))
        streams[hnd].queueing = 0;
}

static size_t snd_stream_fill(snd_stream_hnd_t hnd, uint32_t offset, size_t size);

static void snd_stream_start_type(snd_stream_hnd_t hnd, uint32_t type, uint32_t freq, int st)
{
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    if (!check_hnd(hnd))
        return;
    if (!streams[hnd].get_data && !streams[hnd].req_data)
        return;

    streams[hnd].type = (int)type;
    streams[hnd].channels = st ? 2 : 1;
    streams[hnd].frequency = (int)freq;

    if (streams[hnd].channels > max_channels)
    {
        dbglog(DBG_ERROR, "snd_stream_start_type: initted only for mono\n");
        return;
    }

    if (type == AICA_SM_16BIT)
    {
        streams[hnd].bitsize = 16;
        if (streams[hnd].buffer_size > SND_STREAM_BUFFER_MAX_PCM16)
            streams[hnd].buffer_size = SND_STREAM_BUFFER_MAX_PCM16;
    }
    else if (type == AICA_SM_8BIT)
    {
        streams[hnd].bitsize = 8;
        if (streams[hnd].buffer_size > SND_STREAM_BUFFER_MAX_PCM8)
            streams[hnd].buffer_size = SND_STREAM_BUFFER_MAX_PCM8;
    }
    else if (type == AICA_SM_ADPCM_LS)
    {
        streams[hnd].bitsize = 4;
        if (streams[hnd].buffer_size > SND_STREAM_BUFFER_MAX_ADPCM)
            streams[hnd].buffer_size = SND_STREAM_BUFFER_MAX_ADPCM;
    }

    /* prefill both halves */
    snd_stream_fill(hnd, 0, streams[hnd].buffer_size / 2);
    snd_stream_fill(hnd, (uint32_t)(streams[hnd].buffer_size / 2), streams[hnd].buffer_size / 2);
    streams[hnd].last_write_pos = 0;

    /* make sure these are sync'd (and/or delayed) */
    snd_sh4_to_aica_stop();

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = (uint32_t)streams[hnd].ch[0];
    chan->cmd = AICA_CH_CMD_START | AICA_CH_START_DELAY;
    chan->base = streams[hnd].spu_ram_sch[0];
    chan->type = type;
    chan->length = (uint32_t)bytes_to_samples(hnd, streams[hnd].buffer_size);
    chan->loop = 1;
    chan->loopstart = 0;
    chan->loopend = chan->length - 1;   /* v2.2.1 (later KOS: length) */
    chan->freq = freq;
    chan->vol = 255;
    chan->pan = streams[hnd].channels == 2 ? 0 : 128;
    snd_sh4_to_aica(tmp, cmd->size);

    if (streams[hnd].channels == 2)
    {
        cmd->cmd_id = (uint32_t)streams[hnd].ch[1];
        chan->base = streams[hnd].spu_ram_sch[1];
        chan->pan = 255;
        snd_sh4_to_aica(tmp, cmd->size);
        cmd->cmd_id = (uint32_t)((1ULL << streams[hnd].ch[0]) | (1ULL << streams[hnd].ch[1]));
    }
    else
    {
        cmd->cmd_id = (uint32_t)(1ULL << streams[hnd].ch[0]);
    }
    chan->cmd = AICA_CH_CMD_START | AICA_CH_START_SYNC;
    snd_sh4_to_aica(tmp, cmd->size);

    if (!streams[hnd].queueing)
        snd_sh4_to_aica_start();
}

void snd_stream_start(snd_stream_hnd_t hnd, uint32_t freq, int st)
{
    snd_stream_start_type(hnd, AICA_SM_16BIT, freq, st);
}

void snd_stream_start_pcm8(snd_stream_hnd_t hnd, uint32_t freq, int st)
{
    snd_stream_start_type(hnd, AICA_SM_8BIT, freq, st);
}

void snd_stream_start_adpcm(snd_stream_hnd_t hnd, uint32_t freq, int st)
{
    snd_stream_start_type(hnd, AICA_SM_ADPCM_LS, freq, st);
}

void snd_stream_queue_go(snd_stream_hnd_t hnd)
{
    if (check_hnd(hnd))
        snd_sh4_to_aica_start();
}

void snd_stream_stop(snd_stream_hnd_t hnd)
{
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    if (!check_hnd(hnd))
        return;
    if (!streams[hnd].get_data && !streams[hnd].req_data)
        return;

    if (streams[hnd].channels == 2)
        snd_sh4_to_aica_stop();

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = (uint32_t)streams[hnd].ch[0];
    chan->cmd = AICA_CH_CMD_STOP;
    snd_sh4_to_aica(tmp, cmd->size);

    if (streams[hnd].channels == 2)
    {
        cmd->cmd_id = (uint32_t)streams[hnd].ch[1];
        snd_sh4_to_aica(tmp, cmd->size);
        snd_sh4_to_aica_start();
    }
}

/* The Dreamcast DMAs the buffer(s) to sound RAM and the completion
 * releases stream_sem; the copy here is synchronous, so it releases it
 * at once. */
static int snd_stream_transfer(strchan_t *stream, void *first_buf, uint32_t offset, size_t size)
{
    spu_memload(stream->spu_ram_sch[0] + offset, first_buf, size);
    if (stream->channels == 2)
        spu_memload(stream->spu_ram_sch[1] + offset, sep_buffer[1], size);
    sem_signal(&stream_sem);
    return 0;
}

static size_t snd_stream_fill(snd_stream_hnd_t hnd, uint32_t offset, size_t size)
{
    strchan_t *stream = &streams[hnd];
    const int chans = stream->channels;
    const uintptr_t left = stream->spu_ram_sch[0] + offset;
    const uintptr_t right = stream->spu_ram_sch[1] + offset;
    const int needed_bytes = (int)size * chans;
    int got_bytes = 0;
    void *data = NULL;

    if (!check_hnd(hnd) || chans == 0)
        return 0;

    if (stream->req_data)
        got_bytes = (int)stream->req_data(hnd, left, chans == 2 ? right : 0, (size_t)needed_bytes);
    if (got_bytes > 0)
        return (size_t)got_bytes;
    if (stream->get_data)
        data = stream->get_data(hnd, needed_bytes, &got_bytes);

    if (data == NULL || got_bytes == 0)
    {
        if (sep_buffer[0] == NULL)
        {
            spu_memset_sq(left, 0, (size_t)needed_bytes);
        }
        else
        {
            sem_wait(&stream_sem);
            memset(sep_buffer[0], 0, (size_t)(needed_bytes / chans));
            if (chans == 2)
                memset(sep_buffer[1], 0, (size_t)(needed_bytes / chans));
            snd_stream_transfer(stream, sep_buffer[0], offset, (size_t)(needed_bytes / chans));
        }
        return 0;
    }

    if (got_bytes > needed_bytes)
        got_bytes = needed_bytes;

    process_filters(hnd, &data, &got_bytes);

    if (chans == 1)
    {
        got_bytes = (got_bytes + 3) & ~3;
        sem_wait(&stream_sem);
        snd_stream_transfer(stream, data, offset, (size_t)got_bytes);
        return (size_t)got_bytes;
    }

    got_bytes = (got_bytes + 7) & ~7;
    sem_wait(&stream_sem);
    if (stream->bitsize == 16)
        snd_pcm16_split((uint32_t *)data, sep_buffer[0], sep_buffer[1], (size_t)got_bytes);
    else if (stream->bitsize == 8)
        snd_pcm8_split((uint32_t *)data, sep_buffer[0], sep_buffer[1], (size_t)got_bytes);
    else if (stream->bitsize == 4)
        snd_adpcm_split((uint32_t *)data, sep_buffer[0], sep_buffer[1], (size_t)got_bytes);
    snd_stream_transfer(stream, sep_buffer[0], offset, (size_t)(got_bytes / chans));
    return (size_t)got_bytes;
}

int snd_stream_poll(snd_stream_hnd_t hnd)
{
    uint32_t write_pos;
    uint16_t current_play_pos;
    int needed_samples = 0;
    size_t needed_bytes = 0;
    int got_bytes;
    strchan_t *stream;

    if (hnd < 0 || hnd >= SND_STREAM_MAX)
        return -1;
    stream = &streams[hnd];
    if (!stream->initted || (!stream->get_data && !stream->req_data) || stream->channels == 0)
        return -1;

    /* the channel's position, as the firmware mirrors it */
    current_play_pos = (uint16_t)(snd_get_pos((unsigned)stream->ch[0]) & 0xffff);
    needed_bytes = samples_to_bytes(hnd, current_play_pos);
    if (needed_bytes >= stream->buffer_size)
    {
        dbglog(DBG_ERROR, "snd_stream_poll: chan0(%d).pos = %d\n", stream->ch[0], current_play_pos);
        return -1;
    }
    if (needed_bytes & 31)
        current_play_pos &= (uint16_t)~(bytes_to_samples(hnd, 32) - 1);

    if (stream->last_write_pos <= current_play_pos)
    {
        needed_samples = (int)(current_play_pos - stream->last_write_pos - 1);
        needed_samples &= ~(int)(bytes_to_samples(hnd, 2048 / (size_t)stream->channels) - 1);
        needed_bytes = samples_to_bytes(hnd, (size_t)needed_samples);
        if (needed_bytes < (stream->buffer_size / 2))
            return 0;
    }
    else
    {
        needed_samples = (int)bytes_to_samples(hnd, stream->buffer_size);
        needed_samples -= (int)stream->last_write_pos;
        needed_bytes = samples_to_bytes(hnd, (size_t)needed_samples);
    }

    if (needed_samples <= 0)
        return 0;
    if (needed_bytes > stream->buffer_size / 2)
        needed_bytes = stream->buffer_size / 2;
    if (!stream->initted)
        return -2;

    write_pos = (uint32_t)samples_to_bytes(hnd, stream->last_write_pos);
    got_bytes = (int)snd_stream_fill(hnd, write_pos, needed_bytes);
    if (got_bytes == 0)
        return -3;

    needed_samples = (int)bytes_to_samples(hnd, (size_t)(got_bytes / stream->channels));
    stream->last_write_pos += (uint32_t)needed_samples;
    write_pos = (uint32_t)bytes_to_samples(hnd, stream->buffer_size);
    if (stream->last_write_pos >= write_pos)
        stream->last_write_pos -= write_pos;
    return 0;
}

void snd_stream_volume(snd_stream_hnd_t hnd, int vol)
{
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    if (!check_hnd(hnd))
        return;
    if (streams[hnd].channels == 2)
        snd_sh4_to_aica_stop();

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = (uint32_t)streams[hnd].ch[0];
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_VOL;
    chan->vol = (uint32_t)vol;
    snd_sh4_to_aica(tmp, cmd->size);

    if (streams[hnd].channels == 2)
    {
        cmd->cmd_id = (uint32_t)streams[hnd].ch[1];
        snd_sh4_to_aica(tmp, cmd->size);
        snd_sh4_to_aica_start();
    }
}

void snd_stream_pan(snd_stream_hnd_t hnd, int left_pan, int right_pan)
{
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    if (!check_hnd(hnd))
        return;
    if (streams[hnd].channels == 2)
        snd_sh4_to_aica_stop();

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = (uint32_t)streams[hnd].ch[0];
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_PAN;
    chan->pan = (uint32_t)left_pan;
    snd_sh4_to_aica(tmp, cmd->size);

    if (streams[hnd].channels == 2)
    {
        cmd->cmd_id = (uint32_t)streams[hnd].ch[1];
        chan->pan = (uint32_t)right_pan;
        snd_sh4_to_aica(tmp, cmd->size);
        snd_sh4_to_aica_start();
    }
}
