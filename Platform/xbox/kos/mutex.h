/* kos/mutex.h -- Xbox shim: KallistiOS mutexes.
 *
 * The port declares its locks with MUTEX_INITIALIZER at file scope and
 * never calls mutex_init on them, so the shim initialises lazily: the
 * first lock claims the object (a CAS on `state`) and builds a kernel
 * critical section in `cs`. Recursive, as an Xbox critical section is;
 * KOS's default MUTEX_TYPE_NORMAL is only stricter about misuse, which
 * the port does not commit. */
#ifndef XBOX_KOS_MUTEX_H
#define XBOX_KOS_MUTEX_H

#include <kos/cdefs.h>

__BEGIN_DECLS

typedef struct kos_mutex {
    volatile long state;     /* 0 = fresh, 1 = being built, 2 = ready */
    int type;
    /* RTL_CRITICAL_SECTION is 28 bytes on the Xbox; kept opaque so this
     * header needs no <xtl.h>. */
    uint32_t cs[8];
} mutex_t;

#define MUTEX_TYPE_NORMAL     0
#define MUTEX_TYPE_OLDNORMAL  1
#define MUTEX_TYPE_ERRORCHECK 2
#define MUTEX_TYPE_RECURSIVE  3
#define MUTEX_TYPE_DEFAULT    MUTEX_TYPE_NORMAL

#define MUTEX_INITIALIZER           { 0, MUTEX_TYPE_NORMAL, { 0 } }
#define RECURSIVE_MUTEX_INITIALIZER { 0, MUTEX_TYPE_RECURSIVE, { 0 } }

int mutex_init(mutex_t *m, int mtype);
int mutex_destroy(mutex_t *m);
int mutex_lock(mutex_t *m);
int mutex_trylock(mutex_t *m);
int mutex_unlock(mutex_t *m);
int mutex_is_locked(mutex_t *m);

/* KallistiOS v2.2.1's scope-managed lock, verbatim: locked here, unlocked
 * when execution leaves the enclosing block (src/audio/sndwav.c). */
static inline void __mutex_scoped_cleanup(mutex_t **m) {
    if(*m)
        mutex_unlock(*m);
}

#define ___mutex_lock_scoped(m, l) \
    mutex_t *__scoped_mutex_##l __attribute__((cleanup(__mutex_scoped_cleanup))) = mutex_lock(m) ? NULL : (m)

#define __mutex_lock_scoped(m, l) ___mutex_lock_scoped(m, l)

#define mutex_lock_scoped(m) __mutex_lock_scoped((m), __LINE__)

__END_DECLS

#endif /* XBOX_KOS_MUTEX_H */
