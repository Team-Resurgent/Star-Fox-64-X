/* arch/timer.h -- Xbox shim: KallistiOS's monotonic clocks.
 * Backed by KeQueryPerformanceCounter (Platform/xbox/kos_sys_xbox.c). */
#ifndef XBOX_ARCH_TIMER_H
#define XBOX_ARCH_TIMER_H

#include <kos/cdefs.h>

__BEGIN_DECLS

uint64_t timer_ms_gettime64(void);
uint64_t timer_us_gettime64(void);
uint64_t timer_ns_gettime64(void);
void timer_spin_sleep(int ms);

__END_DECLS

#endif /* XBOX_ARCH_TIMER_H */
