/* kos/thread.h -- Xbox shim: KallistiOS threads over Xbox kernel threads.
 *
 * Same API and the same priority sense as KOS (a LOWER number runs first;
 * PRIO_DEFAULT is 10), mapped onto Xbox thread priorities in
 * Platform/xbox/kos_thread_xbox.c. kthread_t is opaque to the port: it only
 * passes the pointer back (thd_join, thd_set_prio, thd_set_label). */
#ifndef XBOX_KOS_THREAD_H
#define XBOX_KOS_THREAD_H

#include <kos/cdefs.h>
#include <arch/types.h>
#include <arch/stack.h>

__BEGIN_DECLS

#define PRIO_MAX     4096
#define PRIO_DEFAULT 10

/* KOS's public fields, by KOS's names (src/dc/assetroot.c reads `tid`);
 * the Xbox side is private to kos_thread_xbox.c. */
typedef struct kthread {
    tid_t tid;
    prio_t prio;
    char label[256];
    /* private */
    void *handle;               /* HANDLE of the Xbox thread */
    uint32_t xbox_tid;          /* its kernel thread id */
    void *(*routine)(void *);
    void *param;
    void *rv;
    bool detached;
    volatile long state;
    struct kthread *next;
} kthread_t;

typedef struct kthread_attr {
    bool create_detached;
    size_t stack_size;
    void *stack_ptr;
    prio_t prio;
    const char *label;
    bool disable_tls;
} kthread_attr_t;

kthread_t *thd_create(bool detach, void *(*routine)(void *param), void *param);
kthread_t *thd_create_ex(const kthread_attr_t *__RESTRICT attr,
                         void *(*routine)(void *param), void *param);
int thd_destroy(kthread_t *thd);
void thd_exit(void *rv) __noreturn;
void thd_pass(void);
void thd_sleep(unsigned ms);
int thd_set_prio(kthread_t *thd, prio_t prio);
prio_t thd_get_prio(const kthread_t *thd);
tid_t thd_get_id(const kthread_t *thd);
kthread_t *thd_get_current(void);
const char *thd_get_label(const kthread_t *thd);
void thd_set_label(kthread_t *__RESTRICT thd, const char *__RESTRICT label);
int thd_set_hz(unsigned int hertz);
unsigned thd_get_hz(void);
int thd_join(kthread_t *thd, void **value_ptr);
int thd_detach(kthread_t *thd);
int thd_each(int (*cb)(kthread_t *thd, void *user_data), void *data);

__END_DECLS

#endif /* XBOX_KOS_THREAD_H */
