// arch/context.h (x86_64)
// Low-level pieces the scheduler needs from the architecture

#ifndef _KERNEL_ARCH_CONTEXT_H
#define _KERNEL_ARCH_CONTEXT_H

#include <stdint.h>

// Save the callee-saved registers on the current stack, store the stack
// pointer in *old_sp, switch to new_sp and restore that thread's registers.
// Returns when the old thread is switched back to.
void arch_switch_context(uintptr_t *old_sp, uintptr_t new_sp);

// Prepare a new thread's stack so that the first arch_switch_context() to
// it starts entry(arg) with interrupts enabled. Returns the initial sp.
uintptr_t arch_thread_stack_init(uintptr_t stack_top, void (*entry)(void *), void *arg);

#endif
