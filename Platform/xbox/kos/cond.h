/* kos/cond.h -- Xbox shim: KallistiOS condition variables.
 *
 * The Xbox kernel has no condition variable. This one is built from a
 * kernel semaphore and a waiter count guarded by the caller's mutex,
 * created lazily so COND_INITIALIZER at file scope works the way the port
 * uses it (Platform/xbox/kos_thread_xbox.c). */
#ifndef XBOX_KOS_COND_H
#define XBOX_KOS_COND_H

#include <kos/cdefs.h>
#include <kos/mutex.h>

__BEGIN_DECLS

typedef struct kos_condvar {
    volatile long state;     /* 0 = fresh, 1 = being built, 2 = ready */
    volatile long waiters;   /* threads inside cond_wait */
    volatile long to_wake;   /* signals posted and not yet consumed */
    void *sem;               /* HANDLE */
} condvar_t;

#define COND_INITIALIZER { 0, 0, 0, NULL }

int cond_init(condvar_t *cv);
int cond_destroy(condvar_t *cv);
int cond_wait(condvar_t *cv, mutex_t *m);
int cond_wait_timed(condvar_t *cv, mutex_t *m, int timeout);
int cond_signal(condvar_t *cv);
int cond_broadcast(condvar_t *cv);

__END_DECLS

#endif /* XBOX_KOS_COND_H */
