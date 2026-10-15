/* arch/rtc.h -- Xbox shim: the real-time clock, as seconds since 1970
 * (KeQuerySystemTime, Platform/xbox/kos_sys_xbox.c). */
#ifndef XBOX_ARCH_RTC_H
#define XBOX_ARCH_RTC_H

#include <kos/cdefs.h>
#include <time.h>

__BEGIN_DECLS

time_t rtc_unix_secs(void);

__END_DECLS

#endif /* XBOX_ARCH_RTC_H */
