// context.c (x86_64)
// <kernel/arch/context.h>

#include <kernel/arch/context.h>
#include <stdint.h>

extern void arch_thread_start(void);

uintptr_t arch_thread_stack_init(uintptr_t stack_top, void (*entry)(void *), void *arg)
{
    uint64_t *sp = (uint64_t *)(stack_top & ~(uintptr_t)15);

    // arch_switch_context pops r15, r14, r13, r12, rbp, rbx and returns.
    // Returning to arch_thread_start leaves rsp 16-byte aligned, so its
    // call to entry follows the ABI's stack alignment.
    *--sp = (uint64_t)(uintptr_t)arch_thread_start;  // return address
    *--sp = 0;                                       // rbx
    *--sp = 0;                                       // rbp (ends stack traces)
    *--sp = (uint64_t)(uintptr_t)entry;              // r12
    *--sp = (uint64_t)(uintptr_t)arg;                // r13
    *--sp = 0;                                       // r14
    *--sp = 0;                                       // r15

    return (uintptr_t)sp;
}
