/* dc/wdt.h -- Xbox port of KallistiOS's SH4 watchdog-timer API.
 *
 * Same names, types and behaviour as KOS's dc/wdt.h (Falco Girgis, BSD-style
 * KOS licence, see LICENSE.KOS). The SH4's watchdog is a CPU peripheral; the
 * Xbox has none, so Platform/xbox/kos_wdt_xbox.c runs the interval-timer mode
 * on a time-critical thread: the counter advances with the clock, each
 * overflow is one 41 us "interrupt", and the callback runs once the requested
 * interval has elapsed, as KOS's wdt_isr does. Watchdog (reset) mode returns
 * the Xbox to the dashboard on overflow, the nearest thing it has to a reset.
 *
 * Star Fox uses it as the audio timer (include/sf64audio_provisional.h:
 * audio_timer_setup = wdt_enable_timer, kick = wdt_pet, reset = wdt_disable).
 */
#ifndef XBOX_DC_WDT_H
#define XBOX_DC_WDT_H

#include <kos/cdefs.h>
__BEGIN_DECLS

#include <stdint.h>

typedef enum WDT_CLK_DIV {
    WDT_CLK_DIV_32,     /* Period: 41us */
    WDT_CLK_DIV_64,     /* Period: 82us */
    WDT_CLK_DIV_128,    /* Period: 164us */
    WDT_CLK_DIV_256,    /* Period: 328us */
    WDT_CLK_DIV_512,    /* Period: 656us */
    WDT_CLK_DIV_1024,   /* Period: 1.31ms */
    WDT_CLK_DIV_2048,   /* Period: 2.62ms */
    WDT_CLK_DIV_4096    /* Period: 5.25ms */
} WDT_CLK_DIV;

typedef enum WDT_RST {
    WDT_RST_POWER_ON,   /* Power-On Reset */
    WDT_RST_MANUAL      /* Manual Reset */
} WDT_RST;

typedef void (*wdt_callback)(void *user_data);

void wdt_enable_timer(uint8_t initial_count,
                      uint32_t micro_seconds,
                      uint8_t irq_prio,
                      wdt_callback callback,
                      void *user_data);

void wdt_enable_watchdog(uint8_t initial_count,
                         WDT_CLK_DIV clk_config,
                         WDT_RST reset_select);

uint8_t wdt_get_counter(void);
void wdt_set_counter(uint8_t value);
void wdt_pet(void);
void wdt_disable(void);
int wdt_is_enabled(void);

__END_DECLS

#endif /* XBOX_DC_WDT_H */
