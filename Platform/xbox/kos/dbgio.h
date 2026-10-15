/* kos/dbgio.h -- Xbox shim: KallistiOS's debug-console switch. Off = the
 * debug log (kos/dbglog.h) writes nothing (Platform/xbox/kos_log_xbox.c). */
#ifndef XBOX_KOS_DBGIO_H
#define XBOX_KOS_DBGIO_H

#include <kos/cdefs.h>

__BEGIN_DECLS

void dbgio_enable(void);
void dbgio_disable(void);

__END_DECLS

#endif /* XBOX_KOS_DBGIO_H */
