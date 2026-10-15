/* kos/dbglog.h -- Xbox shim: KallistiOS's leveled debug log.
 *
 * Same levels and the same filter as KOS. The text goes to the xbdm debug
 * channel through a ring drained by a low-priority thread
 * (Platform/xbox/kos_log_xbox.c), never from the caller: the xbdm channel
 * blocks once its buffer fills, and a log line must not be able to stall a
 * frame. dbgio_disable() (the release build calls it first thing) turns
 * every line off, as on the Dreamcast. */
#ifndef XBOX_KOS_DBGLOG_H
#define XBOX_KOS_DBGLOG_H

#include <kos/cdefs.h>

__BEGIN_DECLS

#define DBG_DEAD        0
#define DBG_CRITICAL    1
#define DBG_ERROR       2
#define DBG_WARNING     3
#define DBG_NOTICE      4
#define DBG_INFO        5
#define DBG_DEBUG       6
#define DBG_KDEBUG      7
#define DBG_MAX         8

extern int dbglog_level;

void dbglog_set_level(int level);
void kos_dbglog_printf(int lvl, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#define dbglog(lvl, ...) \
    do { \
        int __dbglog_lvl = (lvl); \
        if (__dbglog_lvl <= dbglog_level) \
            kos_dbglog_printf(__dbglog_lvl, __VA_ARGS__); \
    } while (0)

__END_DECLS

#endif /* XBOX_KOS_DBGLOG_H */
