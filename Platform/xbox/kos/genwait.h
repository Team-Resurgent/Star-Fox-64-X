/* kos/genwait.h -- Xbox shim: KallistiOS's generic wait queues
 * (Platform/xbox/kos_extra_xbox.c).
 *
 * Any address is a queue. genwait_wait blocks the caller on `obj` until a
 * genwait_wake_* names the same address, or `timeout` ms pass (0 = no
 * limit). Waiters are woken oldest first. Same signature as the KOS the
 * Dreamcast port builds against (2.2.x: four arguments). */
#ifndef XBOX_KOS_GENWAIT_H
#define XBOX_KOS_GENWAIT_H

#include <kos/cdefs.h>

__BEGIN_DECLS

/* 0 when woken; -1 (errno EAGAIN) when the timeout ran out, after calling
 * `callback(obj)` if one was given */
int genwait_wait(void *obj, const char *mesg, int timeout, void (*callback)(void *));
/* wake up to `cnt` waiters (all if cnt <= 0), handing them `err` as their
 * errno-style result; returns how many were woken */
int genwait_wake_cnt(void *obj, int cnt, int err);
void genwait_wake_all(void *obj);
void genwait_wake_one(void *obj);
void genwait_wake_all_err(void *obj, int err);
void genwait_wake_one_err(void *obj, int err);

__END_DECLS

#endif /* XBOX_KOS_GENWAIT_H */
