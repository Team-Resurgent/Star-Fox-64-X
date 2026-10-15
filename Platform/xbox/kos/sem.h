/* kos/sem.h -- Xbox shim: KallistiOS counting semaphores over kernel
 * semaphores (Platform/xbox/kos_thread_xbox.c). */
#ifndef XBOX_KOS_SEM_H
#define XBOX_KOS_SEM_H

#include <kos/cdefs.h>

__BEGIN_DECLS

typedef struct kos_semaphore {
    int initialized;
    void *handle;            /* HANDLE */
} semaphore_t;

#define SEM_INITIALIZER(value) { 0, NULL }

int sem_init(semaphore_t *sm, int count);
int sem_destroy(semaphore_t *sm);
int sem_wait(semaphore_t *sm);
int sem_trywait(semaphore_t *sm);
int sem_wait_timed(semaphore_t *sm, int timeout);
int sem_signal(semaphore_t *sm);

__END_DECLS

#endif /* XBOX_KOS_SEM_H */
