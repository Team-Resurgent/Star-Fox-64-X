/* kos/oneshot_timer.h -- Xbox shim: KallistiOS's one-shot timers, built as
 * KOS builds them: a worker thread that waits `timeout_ms` on the timer and
 * calls the callback unless the wait was ended early by a stop.
 * (Platform/xbox/kos_extra_xbox.c) */
#ifndef XBOX_KOS_ONESHOT_TIMER_H
#define XBOX_KOS_ONESHOT_TIMER_H

#include <kos/cdefs.h>

__BEGIN_DECLS

typedef struct oneshot_timer oneshot_timer_t;

oneshot_timer_t *oneshot_timer_create(void (*cb)(void *), void *data,
                                      unsigned int timeout_ms);
void oneshot_timer_destroy(oneshot_timer_t *timer);
void oneshot_timer_setup(oneshot_timer_t *timer, void (*cb)(void *),
                         void *data, unsigned int timeout_ms);
void oneshot_timer_start(oneshot_timer_t *timer);
void oneshot_timer_stop(oneshot_timer_t *timer);

static inline void oneshot_timer_reset(oneshot_timer_t *timer) {
    oneshot_timer_stop(timer);
    oneshot_timer_start(timer);
}

__END_DECLS

#endif /* XBOX_KOS_ONESHOT_TIMER_H */
