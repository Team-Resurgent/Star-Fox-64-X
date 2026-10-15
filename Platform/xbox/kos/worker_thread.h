/* kos/worker_thread.h -- Xbox shim: KallistiOS's worker threads, with the
 * job queue of the KOS release the Dreamcast port builds against (2.2.x:
 * kthread_job_t, thd_worker_add_job, thd_worker_dequeue_job).
 *
 * A worker is a thread that sleeps until thd_worker_wakeup and then calls
 * its routine once; a wakeup while the routine runs makes it run again.
 * Jobs are queued by the caller and taken by the routine.
 * (Platform/xbox/kos_extra_xbox.c) */
#ifndef XBOX_KOS_WORKER_THREAD_H
#define XBOX_KOS_WORKER_THREAD_H

#include <kos/cdefs.h>
#include <kos/thread.h>

__BEGIN_DECLS

typedef struct kthread_job {
    struct kthread_job *next;   /* the queue link (KOS: STAILQ_ENTRY) */
    void *data;
} kthread_job_t;

typedef struct kthread_worker kthread_worker_t;

kthread_worker_t *thd_worker_create_ex(const kthread_attr_t *attr,
                                       void (*routine)(void *), void *data);

static inline kthread_worker_t *thd_worker_create(void (*routine)(void *), void *data) {
    return thd_worker_create_ex(NULL, routine, data);
}

void thd_worker_destroy(kthread_worker_t *thd);
void thd_worker_wakeup(kthread_worker_t *thd);
kthread_t *thd_worker_get_thread(kthread_worker_t *thd);
void thd_worker_add_job(kthread_worker_t *thd, kthread_job_t *job);
kthread_job_t *thd_worker_dequeue_job(kthread_worker_t *thd);

__END_DECLS

#endif /* XBOX_KOS_WORKER_THREAD_H */
