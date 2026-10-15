/* arch/stack.h -- Xbox shim: KallistiOS's thread stack sizes (same values
 * as kernel/arch/dreamcast/include/arch/stack.h). Xbox threads are created
 * with these sizes too (Platform/xbox/kos_thread_xbox.c). */
#ifndef XBOX_ARCH_STACK_H
#define XBOX_ARCH_STACK_H

#define THD_STACK_SIZE        32768
#define THD_KERNEL_STACK_SIZE (64 * 1024)

#endif /* XBOX_ARCH_STACK_H */
