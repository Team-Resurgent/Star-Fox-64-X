/* arch/arch.h -- Xbox shim: the slice of KallistiOS's arch layer the port uses.
 * Implemented in Platform/xbox/kos_sys_xbox.c. */
#ifndef XBOX_ARCH_ARCH_H
#define XBOX_ARCH_ARCH_H

#include <kos/cdefs.h>
#include <arch/types.h>

__BEGIN_DECLS

/* Top of the memory the C heap may grow into. The port sizes its
 * read-ahead budget from it (src/dc/assetroot.c pf_top). On the Xbox the
 * heap is not a single sbrk region; this is the address mm_sbrk(0) can
 * grow to, computed at boot from the console's free physical memory. */
extern uint8_t *_arch_mem_top;

void arch_abort(void) __noreturn;
void arch_exit(void) __noreturn;
void arch_reboot(void) __noreturn;

/* KOS's init flags: meaningless on the Xbox, where nothing is initialised
 * by flag. The port only ever names them under DB_DCLOAD_NET. */
#define KOS_INIT_FLAGS(flags) \
    const uint32_t __kos_init_flags_unused = (flags)
#define INIT_DEFAULT 0
#define INIT_NET     0

__END_DECLS

#endif /* XBOX_ARCH_ARCH_H */
