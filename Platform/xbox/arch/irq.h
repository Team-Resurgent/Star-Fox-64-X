/* arch/irq.h -- Xbox shim.
 *
 * Every irq_disable/irq_restore in the port sits in a debug facility
 * (DB_* flags, off in this build) that brackets a short critical section.
 * The Xbox equivalent of "no other thread runs" is raising to DISPATCH
 * level; the shim does that, so the sections stay atomic if one is ever
 * switched on (Platform/xbox/kos_sys_xbox.c). */
#ifndef XBOX_ARCH_IRQ_H
#define XBOX_ARCH_IRQ_H

#include <kos/cdefs.h>

__BEGIN_DECLS

typedef int irq_mask_t;

irq_mask_t irq_disable(void);
void irq_restore(irq_mask_t v);

#define irq_disable_scoped() \
    irq_mask_t __irq_scoped_##__LINE__ __attribute__((cleanup(irq_scoped_cleanup))) = irq_disable()
void irq_scoped_cleanup(irq_mask_t *state);

__END_DECLS

#endif /* XBOX_ARCH_IRQ_H */
