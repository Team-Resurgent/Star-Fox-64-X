/* kos_thread_xbox.c -- KallistiOS threads and synchronisation on the Xbox
 * kernel: threads, mutexes, counting semaphores, condition variables.
 * See Platform/xbox/kos/{thread,mutex,sem,cond}.h. */
#include "xbox_xtl.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <kos/thread.h>
#include <kos/mutex.h>
#include <kos/sem.h>
#include <kos/cond.h>
#include <kos/dbglog.h>

/* ================================================================ threads
 *
 * Every thread the port creates gets a kthread_t for its whole life; the
 * main thread (and any thread not created here) is registered on first
 * thd_get_current. The list is only walked by thd_each. */

static CRITICAL_SECTION sThdLock;
static volatile LONG sThdLockState;     /* 0 fresh, 1 building, 2 ready */
static kthread_t *sThdList;
static volatile LONG sNextTid = 1;
static DWORD sTlsSlot = (DWORD)-1;
static unsigned sThdHz = 100;

static void thd_lock_init(void)
{
    if (sThdLockState == 2)
        return;
    if (InterlockedCompareExchange((LONG *)&sThdLockState, 1, 0) == 0)
    {
        InitializeCriticalSection(&sThdLock);
        sTlsSlot = TlsAlloc();
        sThdLockState = 2;
    }
    while (sThdLockState != 2)
        Sleep(1);
}

static void thd_list_add(kthread_t *t)
{
    EnterCriticalSection(&sThdLock);
    t->next = sThdList;
    sThdList = t;
    LeaveCriticalSection(&sThdLock);
}

static void thd_list_remove(kthread_t *t)
{
    kthread_t **p;

    EnterCriticalSection(&sThdLock);
    for (p = &sThdList; *p != NULL; p = &(*p)->next)
    {
        if (*p == t)
        {
            *p = t->next;
            break;
        }
    }
    LeaveCriticalSection(&sThdLock);
}

/* KOS: a LOWER number runs first, PRIO_DEFAULT (10) is normal. The port
 * raises its audio thread to PRIO_DEFAULT / 2 and lowers its loader and
 * memory-card writer to PRIO_DEFAULT + 1. */
static int xbox_prio(prio_t prio)
{
    if (prio <= PRIO_DEFAULT / 2)
        return THREAD_PRIORITY_HIGHEST;
    if (prio < PRIO_DEFAULT)
        return THREAD_PRIORITY_ABOVE_NORMAL;
    if (prio == PRIO_DEFAULT)
        return THREAD_PRIORITY_NORMAL;
    return THREAD_PRIORITY_BELOW_NORMAL;
}

void xbox_fpu_setup(void);      /* Platform/xbox/xbox_main.c: SSE flush-to-zero */

static DWORD WINAPI thd_entry(LPVOID p)
{
    kthread_t *t = (kthread_t *)p;

    xbox_fpu_setup();
    TlsSetValue(sTlsSlot, t);
    t->rv = t->routine(t->param);
    thd_exit(t->rv);
    return 0;
}

kthread_t *thd_create_ex(const kthread_attr_t *attr,
                         void *(*routine)(void *param), void *param)
{
    kthread_t *t;
    size_t stack = THD_STACK_SIZE;

    thd_lock_init();
    t = (kthread_t *)calloc(1, sizeof(*t));
    if (t == NULL)
        return NULL;
    t->tid = (tid_t)InterlockedIncrement((LONG *)&sNextTid);
    t->prio = PRIO_DEFAULT;
    t->routine = routine;
    t->param = param;
    strcpy(t->label, "[user]");
    if (attr != NULL)
    {
        if (attr->stack_size)
            stack = attr->stack_size;
        if (attr->prio)
            t->prio = attr->prio;
        if (attr->label)
        {
            strncpy(t->label, attr->label, sizeof(t->label) - 1);
            t->label[sizeof(t->label) - 1] = '\0';
        }
        t->detached = attr->create_detached;
        /* attr->stack_ptr: only the debug stack guard hands one in
         * (DB_STACK_GUARD); an Xbox thread always gets a kernel stack. */
    }
    /* The port sizes some stacks for SH-4 frames (src/dc/sysshim.c gives
     * each GObj coroutine 16 KB); x86 code at -O3, with clang's inlining,
     * can need more for the same calls, and an overflow here is a crash,
     * not a slowdown. 32 KB is the floor. */
    if (stack < 32 * 1024)
        stack = 32 * 1024;
    thd_list_add(t);
    t->handle = CreateThread(NULL, (DWORD)stack, thd_entry, t, CREATE_SUSPENDED,
                             (LPDWORD)&t->xbox_tid);
    if (t->handle == NULL)
    {
        thd_list_remove(t);
        free(t);
        return NULL;
    }
    SetThreadPriority((HANDLE)t->handle, xbox_prio(t->prio));
    ResumeThread((HANDLE)t->handle);
    return t;
}

kthread_t *thd_create(bool detach, void *(*routine)(void *param), void *param)
{
    kthread_attr_t attr;

    memset(&attr, 0, sizeof(attr));
    attr.create_detached = detach;
    return thd_create_ex(&attr, routine, param);
}

void thd_exit(void *rv)
{
    kthread_t *t = thd_get_current();

    if (t != NULL)
    {
        t->rv = rv;
        t->state = 1;           /* finished: thd_join may reap it */
        if (t->detached)
        {
            /* nobody joins a detached thread: it reaps itself */
            thd_list_remove(t);
            CloseHandle((HANDLE)t->handle);
            free(t);
        }
    }
    ExitThread(0);
    for (;;)
        ;
}

int thd_join(kthread_t *thd, void **value_ptr)
{
    if (thd == NULL || thd->detached || thd == thd_get_current())
        return -1;
    WaitForSingleObject((HANDLE)thd->handle, INFINITE);
    if (value_ptr)
        *value_ptr = thd->rv;
    thd_list_remove(thd);
    CloseHandle((HANDLE)thd->handle);
    free(thd);
    return 0;
}

int thd_detach(kthread_t *thd)
{
    if (thd == NULL || thd->detached)
        return -1;
    thd->detached = true;
    return 0;
}

int thd_destroy(kthread_t *thd)
{
    /* KOS kills the thread outright. Nothing in the port calls this; a
     * forced termination of an Xbox thread would leak whatever it holds,
     * so it is refused rather than half-done. */
    (void)thd;
    dbglog(DBG_ERROR, "thd_destroy: not supported on the Xbox\n");
    return -1;
}

void thd_pass(void)
{
    Sleep(0);
}

void thd_sleep(unsigned ms)
{
    Sleep(ms);
}

int thd_set_prio(kthread_t *thd, prio_t prio)
{
    if (thd == NULL)
        return -1;
    thd->prio = prio;
    if (thd->handle != NULL)
        SetThreadPriority((HANDLE)thd->handle, xbox_prio(prio));
    return 0;
}

prio_t thd_get_prio(const kthread_t *thd)
{
    return thd ? thd->prio : PRIO_DEFAULT;
}

tid_t thd_get_id(const kthread_t *thd)
{
    return thd ? thd->tid : 0;
}

kthread_t *thd_get_current(void)
{
    kthread_t *t;

    thd_lock_init();
    t = (kthread_t *)TlsGetValue(sTlsSlot);
    if (t == NULL)
    {
        /* a thread this file did not create (the main thread): give it a
         * kthread_t of its own, once */
        t = (kthread_t *)calloc(1, sizeof(*t));
        if (t == NULL)
            return NULL;
        t->tid = (tid_t)InterlockedIncrement((LONG *)&sNextTid);
        t->prio = PRIO_DEFAULT;
        t->xbox_tid = GetCurrentThreadId();
        t->handle = NULL;       /* not ours to wait on or close */
        t->detached = true;
        strcpy(t->label, "[kernel]");
        TlsSetValue(sTlsSlot, t);
        thd_list_add(t);
    }
    return t;
}

const char *thd_get_label(const kthread_t *thd)
{
    return thd ? thd->label : "";
}

void thd_set_label(kthread_t *__RESTRICT thd, const char *__RESTRICT label)
{
    if (thd == NULL || label == NULL)
        return;
    strncpy(thd->label, label, sizeof(thd->label) - 1);
    thd->label[sizeof(thd->label) - 1] = '\0';
}

/* KOS's scheduler tick. The port raises it to 1000 Hz so its audio thread's
 * thd_sleep(1) really sleeps one millisecond; the Xbox kernel's clock
 * interrupt is already 1 ms, so the request is met as it stands. */
int thd_set_hz(unsigned int hertz)
{
    if (hertz == 0 || hertz > 1000)
        return -1;
    sThdHz = hertz;
    return 0;
}

unsigned thd_get_hz(void)
{
    return sThdHz;
}

int thd_each(int (*cb)(kthread_t *thd, void *user_data), void *data)
{
    kthread_t *t;
    int rv = 0;

    thd_lock_init();
    EnterCriticalSection(&sThdLock);
    for (t = sThdList; t != NULL; t = t->next)
    {
        if ((rv = cb(t, data)) != 0)
            break;
    }
    LeaveCriticalSection(&sThdLock);
    return rv;
}

/* ================================================================ mutexes
 * Lazy: MUTEX_INITIALIZER objects are first touched by mutex_lock. The CAS
 * makes the first-use race between two threads safe. */

_Static_assert(sizeof(((mutex_t *)0)->cs) >= sizeof(CRITICAL_SECTION),
               "mutex_t has no room for a CRITICAL_SECTION");

static CRITICAL_SECTION *mutex_cs(mutex_t *m)
{
    if (m->state != 2)
    {
        if (InterlockedCompareExchange((LONG *)&m->state, 1, 0) == 0)
        {
            InitializeCriticalSection((CRITICAL_SECTION *)m->cs);
            m->state = 2;
        }
        while (m->state != 2)
            Sleep(1);
    }
    return (CRITICAL_SECTION *)m->cs;
}

int mutex_init(mutex_t *m, int mtype)
{
    memset(m, 0, sizeof(*m));
    m->type = mtype;
    mutex_cs(m);
    return 0;
}

int mutex_destroy(mutex_t *m)
{
    if (m->state == 2)
        DeleteCriticalSection((CRITICAL_SECTION *)m->cs);
    m->state = 0;
    return 0;
}

int mutex_lock(mutex_t *m)
{
    EnterCriticalSection(mutex_cs(m));
    return 0;
}

int mutex_trylock(mutex_t *m)
{
    return TryEnterCriticalSection(mutex_cs(m)) ? 0 : -1;
}

int mutex_unlock(mutex_t *m)
{
    LeaveCriticalSection(mutex_cs(m));
    return 0;
}

int mutex_is_locked(mutex_t *m)
{
    CRITICAL_SECTION *cs = mutex_cs(m);

    if (!TryEnterCriticalSection(cs))
        return 1;
    LeaveCriticalSection(cs);
    return 0;
}

/* ============================================================ semaphores */

int sem_init(semaphore_t *sm, int count)
{
    if (count < 0)
        return -1;
    sm->handle = CreateSemaphore(NULL, count, LONG_MAX, NULL);
    sm->initialized = sm->handle != NULL;
    return sm->initialized ? 0 : -1;
}

int sem_destroy(semaphore_t *sm)
{
    if (sm->initialized)
        CloseHandle((HANDLE)sm->handle);
    sm->initialized = 0;
    sm->handle = NULL;
    return 0;
}

int sem_wait(semaphore_t *sm)
{
    return WaitForSingleObject((HANDLE)sm->handle, INFINITE) == WAIT_OBJECT_0 ? 0 : -1;
}

int sem_trywait(semaphore_t *sm)
{
    return WaitForSingleObject((HANDLE)sm->handle, 0) == WAIT_OBJECT_0 ? 0 : -1;
}

int sem_wait_timed(semaphore_t *sm, int timeout)
{
    return WaitForSingleObject((HANDLE)sm->handle,
                               timeout > 0 ? (DWORD)timeout : INFINITE)
           == WAIT_OBJECT_0 ? 0 : -1;
}

int sem_signal(semaphore_t *sm)
{
    return ReleaseSemaphore((HANDLE)sm->handle, 1, NULL) ? 0 : -1;
}

/* ================================================ condition variables
 *
 * A kernel semaphore that signals post into, plus two counters: the
 * threads waiting and the wakeups posted and not yet taken. A signal only
 * posts when there is a waiter without a wakeup already on its way, so a
 * signal with nobody waiting is lost -- the condition-variable contract,
 * which the port's loops (`while (!x) cond_wait(...)`) are written for.
 * The counters are guarded by one lock for all condition variables; it is
 * held for a handful of instructions. */

static CRITICAL_SECTION sCondLock;
static volatile LONG sCondLockState;

static void cond_lock(void)
{
    if (sCondLockState != 2)
    {
        if (InterlockedCompareExchange((LONG *)&sCondLockState, 1, 0) == 0)
        {
            InitializeCriticalSection(&sCondLock);
            sCondLockState = 2;
        }
        while (sCondLockState != 2)
            Sleep(1);
    }
    EnterCriticalSection(&sCondLock);
}

static void cond_unlock(void)
{
    LeaveCriticalSection(&sCondLock);
}

static HANDLE cond_sem(condvar_t *cv)
{
    if (cv->state != 2)
    {
        if (InterlockedCompareExchange((LONG *)&cv->state, 1, 0) == 0)
        {
            cv->sem = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
            cv->waiters = 0;
            cv->to_wake = 0;
            cv->state = 2;
        }
        while (cv->state != 2)
            Sleep(1);
    }
    return (HANDLE)cv->sem;
}

int cond_init(condvar_t *cv)
{
    memset(cv, 0, sizeof(*cv));
    return cond_sem(cv) != NULL ? 0 : -1;
}

int cond_destroy(condvar_t *cv)
{
    if (cv->state == 2 && cv->sem != NULL)
        CloseHandle((HANDLE)cv->sem);
    cv->state = 0;
    cv->sem = NULL;
    return 0;
}

int cond_wait_timed(condvar_t *cv, mutex_t *m, int timeout)
{
    HANDLE sem = cond_sem(cv);
    DWORD r;

    cond_lock();
    cv->waiters++;
    cond_unlock();

    mutex_unlock(m);
    r = WaitForSingleObject(sem, timeout > 0 ? (DWORD)timeout : INFINITE);

    cond_lock();
    cv->waiters--;
    if (r == WAIT_OBJECT_0)
    {
        cv->to_wake--;
    }
    else if (cv->to_wake > cv->waiters)
    {
        /* timed out, but a wakeup was posted for this thread meanwhile:
         * take it back so it is not handed to a later waiter */
        WaitForSingleObject(sem, 0);
        cv->to_wake--;
        r = WAIT_OBJECT_0;
    }
    cond_unlock();

    mutex_lock(m);
    return r == WAIT_OBJECT_0 ? 0 : -1;
}

int cond_wait(condvar_t *cv, mutex_t *m)
{
    return cond_wait_timed(cv, m, 0);
}

int cond_signal(condvar_t *cv)
{
    HANDLE sem = cond_sem(cv);

    cond_lock();
    if (cv->waiters > cv->to_wake)
    {
        cv->to_wake++;
        ReleaseSemaphore(sem, 1, NULL);
    }
    cond_unlock();
    return 0;
}

int cond_broadcast(condvar_t *cv)
{
    HANDLE sem = cond_sem(cv);
    LONG n;

    cond_lock();
    n = cv->waiters - cv->to_wake;
    if (n > 0)
    {
        cv->to_wake += n;
        ReleaseSemaphore(sem, n, NULL);
    }
    cond_unlock();
    return 0;
}
