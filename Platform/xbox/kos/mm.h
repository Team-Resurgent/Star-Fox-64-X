/* kos/mm.h -- Xbox shim: KallistiOS's program break.
 *
 * The Xbox C heap is the kernel's RTL process heap, not an sbrk arena, so
 * there is no break to move. The port only ever asks mm_sbrk(0), to size
 * its read-ahead budget as `_arch_mem_top - THD_KERNEL_STACK_SIZE -
 * mm_sbrk(0)` (src/dc/assetroot.c pf_top). The shim keeps that expression
 * meaning what it means on the Dreamcast -- the RAM the heap can still
 * grow into -- by answering with `_arch_mem_top - THD_KERNEL_STACK_SIZE -
 * <free physical memory now>` (Platform/xbox/kos_sys_xbox.c). Any other
 * increment fails the way an exhausted sbrk does. */
#ifndef XBOX_KOS_MM_H
#define XBOX_KOS_MM_H

#include <kos/cdefs.h>

__BEGIN_DECLS

void *mm_sbrk(ptrdiff_t increment);

__END_DECLS

#endif /* XBOX_KOS_MM_H */
