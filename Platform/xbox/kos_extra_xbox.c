/* kos_extra_xbox.c -- the KallistiOS services the Star Fox port uses beyond
 * the ones it shares with the other Xbox ports: generic wait queues, worker
 * threads and their job queue, one-shot timers, vertical-blank handlers and
 * the VMU framebuffer. See kos/genwait.h, kos/worker_thread.h,
 * kos/oneshot_timer.h, dc/vblank.h, dc/vmu_fb.h.
 *
 * The game leans on these for its pacing: src/sys/sys_main.c adds a vblank
 * handler that counts frames and genwait_wake_all's the counter, and both the
 * render loop (gfx_dc.c swap_buffers_end) and the audio thread genwait_wait
 * on it. */
#include "xbox_xtl.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <kos/genwait.h>
#include <kos/worker_thread.h>
#include <kos/oneshot_timer.h>
#include <kos/dbglog.h>
#include <dc/vblank.h>
#include <dc/vmu_fb.h>

/* =================================================================== lock */

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

/* ================================================================ genwait
 * A table of waiters, each with its own auto-reset event. A wait claims a
 * slot (oldest-first order is kept by a sequence number), a wake marks the
 * matching slots and sets their events. A wake that lands between a slot's
 * timeout and its owner taking the lock still counts as a wake, as KOS's
 * does (the waiter is off the queue only once it has the lock). */

#define GW_SLOTS 64

typedef struct
{
    void *obj;              /* NULL: free */
    HANDLE ev;
    uint32_t seq;
    int woken;
    int err;
} GwSlot;

static GwSlot sGw[GW_SLOTS];
static uint32_t sGwSeq;

/* Wait with the lock already held, as KOS waits with interrupts already
 * off: the caller's check of its condition and this registration are one
 * step, so a wake cannot fall between them. Returns with the lock released. */
static int genwait_wait_locked(void *obj, int timeout, void (*callback)(void *))
{
    GwSlot *s = NULL;
    DWORD r;
    int i, rv, timed_out;

    for (i = 0; i < GW_SLOTS; i++)
    {
        if (sGw[i].obj == NULL)
        {
            s = &sGw[i];
            break;
        }
    }
    if (s == NULL)
    {
        unlock();
        dbglog(DBG_CRITICAL, "genwait_wait: more than %d threads waiting\n", GW_SLOTS);
        Sleep(timeout > 0 ? (DWORD)timeout : 1);
        errno = EAGAIN;
        return -1;
    }
    if (s->ev == NULL)
        s->ev = CreateEvent(NULL, FALSE, FALSE, NULL);
    else
        ResetEvent(s->ev);
    s->obj = obj;
    s->seq = ++sGwSeq;
    s->woken = 0;
    s->err = 0;
    unlock();

    r = WaitForSingleObject(s->ev, timeout > 0 ? (DWORD)timeout : INFINITE);
    (void)r;

    lock();
    timed_out = !s->woken;
    if (!timed_out)
    {
        rv = s->err ? -1 : 0;
        if (s->err)
            errno = s->err;
    }
    else
    {
        rv = -1;
        errno = EAGAIN;
    }
    s->obj = NULL;
    unlock();

    if (timed_out && callback != NULL)
        callback(obj);
    return rv;
}

int genwait_wait(void *obj, const char *mesg, int timeout, void (*callback)(void *))
{
    (void)mesg;
    lock();
    return genwait_wait_locked(obj, timeout, callback);
}

int genwait_wake_cnt(void *obj, int cnt, int err)
{
    int woken = 0;

    lock();
    while (cnt <= 0 || woken < cnt)
    {
        GwSlot *oldest = NULL;
        int i;

        for (i = 0; i < GW_SLOTS; i++)
        {
            GwSlot *s = &sGw[i];

            if (s->obj == obj && !s->woken && (oldest == NULL || (int32_t)(s->seq - oldest->seq) < 0))
                oldest = s;
        }
        if (oldest == NULL)
            break;
        oldest->woken = 1;
        oldest->err = err;
        SetEvent(oldest->ev);
        woken++;
    }
    unlock();
    return woken;
}

void genwait_wake_all(void *obj)
{
    genwait_wake_cnt(obj, 0, 0);
}

void genwait_wake_one(void *obj)
{
    genwait_wake_cnt(obj, 1, 0);
}

void genwait_wake_all_err(void *obj, int err)
{
    genwait_wake_cnt(obj, 0, err);
}

void genwait_wake_one_err(void *obj, int err)
{
    genwait_wake_cnt(obj, 1, err);
}

/* ========================================================= worker threads
 * KOS's worker.c: the thread sleeps until woken, clears `pending`, calls the
 * routine; a wake while the routine runs leaves `pending` set, so it runs
 * again. The job queue is KOS 2.2's: FIFO, the caller owns the job memory. */

struct kthread_worker
{
    kthread_t *thd;
    void (*routine)(void *);
    void *data;
    volatile int pending;
    volatile int quit;
    kthread_job_t *head, *tail;
};

static void *thd_worker_thread(void *d)
{
    kthread_worker_t *w = (kthread_worker_t *)d;

    for (;;)
    {
        lock();
        if (!w->pending && !w->quit)
        {
            genwait_wait_locked(w, 0, NULL);
            lock();
        }
        if (w->quit)
        {
            unlock();
            break;
        }
        w->pending = 0;
        unlock();
        w->routine(w->data);
    }
    free(w);
    return NULL;
}

kthread_worker_t *thd_worker_create_ex(const kthread_attr_t *attr,
                                       void (*routine)(void *), void *data)
{
    kthread_worker_t *w;

    if (routine == NULL)
        return NULL;
    w = (kthread_worker_t *)calloc(1, sizeof(*w));
    if (w == NULL)
        return NULL;
    w->routine = routine;
    w->data = data;
    lock();
    w->thd = thd_create_ex(attr, thd_worker_thread, w);
    unlock();
    if (w->thd == NULL)
    {
        free(w);
        return NULL;
    }
    return w;
}

void thd_worker_wakeup(kthread_worker_t *w)
{
    lock();
    w->pending = 1;
    unlock();
    genwait_wake_one(w);
}

void thd_worker_destroy(kthread_worker_t *w)
{
    kthread_t *thd;

    lock();
    w->quit = 1;
    thd = w->thd;
    unlock();
    genwait_wake_one(w);
    thd_join(thd, NULL);
}

kthread_t *thd_worker_get_thread(kthread_worker_t *w)
{
    return w->thd;
}

/* A job already waiting in the queue is not linked in a second time:
 * that would close the list on itself (KOS's STAILQ would corrupt the same
 * way). The port re-adds a pad's job every frame its rumble flag is up,
 * which can be before the worker has taken the previous one. */
void thd_worker_add_job(kthread_worker_t *w, kthread_job_t *job)
{
    kthread_job_t *j;

    lock();
    for (j = w->head; j != NULL; j = j->next)
    {
        if (j == job)
        {
            unlock();
            return;
        }
    }
    job->next = NULL;
    if (w->tail)
        w->tail->next = job;
    else
        w->head = job;
    w->tail = job;
    unlock();
}

kthread_job_t *thd_worker_dequeue_job(kthread_worker_t *w)
{
    kthread_job_t *job;

    lock();
    job = w->head;
    if (job != NULL)
    {
        w->head = job->next;
        if (w->head == NULL)
            w->tail = NULL;
        job->next = NULL;
    }
    unlock();
    return job;
}

/* ======================================================= one-shot timers
 * KOS's oneshot_timer.c, line for line: a worker that waits on the timer
 * for the timeout and fires the callback when the wait was not ended by a
 * stop. */

struct oneshot_timer
{
    kthread_worker_t *worker;
    void (*cb)(void *);
    void *data;
    unsigned int timeout_ms;
};

static void oneshot_timer_timeout(void *d)
{
    oneshot_timer_t *t = (oneshot_timer_t *)d;

    if (genwait_wait(t, "One-shot timer", (int)t->timeout_ms, NULL) < 0)
        t->cb(t->data);
}

void oneshot_timer_setup(oneshot_timer_t *t, void (*cb)(void *), void *data,
                         unsigned int timeout_ms)
{
    t->timeout_ms = timeout_ms;
    t->cb = cb;
    t->data = data;
}

oneshot_timer_t *oneshot_timer_create(void (*cb)(void *), void *data, unsigned int timeout_ms)
{
    oneshot_timer_t *t = (oneshot_timer_t *)malloc(sizeof(*t));

    if (t == NULL)
        return NULL;
    oneshot_timer_setup(t, cb, data, timeout_ms);
    t->worker = thd_worker_create(oneshot_timer_timeout, t);
    if (t->worker == NULL)
    {
        free(t);
        return NULL;
    }
    return t;
}

void oneshot_timer_destroy(oneshot_timer_t *t)
{
    oneshot_timer_stop(t);
    thd_worker_destroy(t->worker);
    free(t);
}

void oneshot_timer_start(oneshot_timer_t *t)
{
    thd_worker_wakeup(t->worker);
}

void oneshot_timer_stop(oneshot_timer_t *t)
{
    genwait_wake_all(t);
}

/* ===================================================== vertical blanks */

#define VBL_HANDLERS 8

typedef struct
{
    vblank_handler_t fn;
    void *data;
} VblHandler;

static VblHandler sVbl[VBL_HANDLERS];
static int sVblThread;

int pvr_xbox_device_ready(void);   /* pvr_xbox.c */

static DWORD WINAPI vblank_thread(LPVOID arg)
{
    LARGE_INTEGER f, t;
    uint64_t next = 0;
    int i;

    (void)arg;
    QueryPerformanceFrequency(&f);
    for (;;)
    {
        if (pvr_xbox_device_ready())
        {
            D3DDevice_BlockUntilVerticalBlank();
        }
        else
        {
            /* no display yet: keep the 59.94 Hz beat on the clock */
            QueryPerformanceCounter(&t);
            if (next == 0)
                next = (uint64_t)t.QuadPart;
            next += (uint64_t)f.QuadPart * 1001u / 60000u;
            while ((uint64_t)t.QuadPart < next)
            {
                Sleep(1);
                QueryPerformanceCounter(&t);
            }
        }
        for (i = 0; i < VBL_HANDLERS; i++)
        {
            VblHandler h;

            lock();
            h = sVbl[i];
            unlock();
            if (h.fn != NULL)
                h.fn(ASIC_EVT_PVR_VBLANK_BEGIN, h.data);
        }
    }
    return 0;
}

int vblank_handler_add(vblank_handler_t hnd, void *data)
{
    int i, rv = -1;

    lock();
    for (i = 0; i < VBL_HANDLERS; i++)
    {
        if (sVbl[i].fn == NULL)
        {
            sVbl[i].fn = hnd;
            sVbl[i].data = data;
            rv = i;
            break;
        }
    }
    if (rv >= 0 && !sVblThread)
    {
        HANDLE th = CreateThread(NULL, 16 * 1024, vblank_thread, NULL, 0, NULL);

        if (th != NULL)
        {
            /* the vblank interrupt outranks every thread on the Dreamcast */
            SetThreadPriority(th, THREAD_PRIORITY_TIME_CRITICAL);
            CloseHandle(th);
            sVblThread = 1;
        }
    }
    unlock();
    return rv;
}

int vblank_handler_remove(int handle)
{
    if (handle < 0 || handle >= VBL_HANDLERS)
        return -1;
    lock();
    sVbl[handle].fn = NULL;
    sVbl[handle].data = NULL;
    unlock();
    return 0;
}

/* ================================================== VMU framebuffer
 * KOS util/vmu_fb.c's drawing. */

#define VMU_GENMASK(h, l) ((0xffffffffu << (l)) & (0xffffffffu >> (31 - (h))))

static uint64_t extract_bits(const uint8_t *data, unsigned int offt, unsigned int w)
{
    uint32_t tmp, lsb, nb_bits;
    uint64_t bits = 0;

    while (w)
    {
        tmp = data[offt / 8];
        if (8 - (offt & 0x7) > w)
            lsb = 8 - (offt & 0x7) - w;
        else
            lsb = 0;
        nb_bits = 8 - (offt & 0x7) - lsb;
        bits <<= nb_bits;
        tmp &= VMU_GENMASK(7 - (offt & 0x7), lsb);
        bits |= tmp >> lsb;
        offt += nb_bits;
        w -= nb_bits;
    }
    return bits;
}

static void insert_bits(uint8_t *data, unsigned int offt, unsigned int w, uint64_t bits)
{
    uint32_t tmp, lsb, nb_bits, mask;

    while (w)
    {
        tmp = data[offt / 8];
        if (8 - (offt & 0x7) > w)
            lsb = 8 - (offt & 0x7) - w;
        else
            lsb = 0;
        nb_bits = 8 - (offt & 0x7) - lsb;
        mask = VMU_GENMASK(7 - (offt & 0x7), lsb);
        tmp &= ~mask;
        tmp |= ((bits >> (w - nb_bits)) << lsb) & mask;
        data[offt / 8] = (uint8_t)tmp;
        offt += nb_bits;
        w -= nb_bits;
    }
}

static void paint_area_strided(vmufb_t *fb, unsigned int x, unsigned int y, unsigned int w,
                               unsigned int h, unsigned int stride, const uint8_t *data)
{
    unsigned int i;

    for (i = 0; i < h; i++)
        insert_bits((uint8_t *)fb->data, (y + i) * VMU_SCREEN_WIDTH + x, w,
                    extract_bits(data, i * stride, w));
}

void vmufb_paint_area(vmufb_t *fb, unsigned int x, unsigned int y, unsigned int w,
                      unsigned int h, const uint8_t *data)
{
    paint_area_strided(fb, x, y, w, h, w, data);
}

static uint8_t bit_reverse8(uint8_t b)
{
    b = (uint8_t)(((b & 0xF0) >> 4) | ((b & 0x0F) << 4));
    b = (uint8_t)(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
    b = (uint8_t)(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
    return b;
}

void vmufb_paint_xbm(vmufb_t *fb, unsigned int x, unsigned int y, unsigned int w,
                     unsigned int h, const uint8_t *xbm_data)
{
    uint8_t buf[48 * 32];
    unsigned int i, wb = (w + 7) / 8;

    for (i = 0; i < h * wb && i < sizeof(buf); i++)
        buf[i] = bit_reverse8(xbm_data[i]);
    paint_area_strided(fb, x, y, w, h, wb * 8, buf);
}

void vmufb_clear(vmufb_t *fb)
{
    memset(fb->data, 0, sizeof(fb->data));
}

void vmufb_clear_area(vmufb_t *fb, unsigned int x, unsigned int y, unsigned int w,
                      unsigned int h)
{
    uint32_t tmp[VMU_SCREEN_WIDTH];

    memset(tmp, 0, sizeof(tmp));
    vmufb_paint_area(fb, x, y, w, h, (const uint8_t *)tmp);
}

void vmufb_present(const vmufb_t *fb, maple_device_t *dev)
{
    /* only an LCD can show it, and the Xbox bus has none to hand out */
    (void)fb;
    if (dev == NULL || !(dev->info.functions & MAPLE_FUNC_LCD))
        return;
}
