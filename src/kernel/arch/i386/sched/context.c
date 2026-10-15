// context.c (i386)
// <kernel/arch/context.h>

#include <kernel/arch/context.h>
#include <stdint.h>

extern void arch_thread_start(void);

uintptr_t arch_thread_stack_init(uintptr_t stack_top, void (*entry)(void *), void *arg)
{
    uint32_t *sp = (uint32_t *)(stack_top & ~(uintptr_t)15);

    // arch_switch_context pops edi, esi, ebx, ebp and returns. Returning
    // to arch_thread_start leaves esp 16-byte aligned, which it keeps for
    // its call to entry.
    *--sp = (uint32_t)(uintptr_t)arch_thread_start;  // return address
    *--sp = 0;                                       // ebp (ends stack traces)
    *--sp = (uint32_t)(uintptr_t)entry;              // ebx
    *--sp = (uint32_t)(uintptr_t)arg;                // esi
    *--sp = 0;                                       // edi

    return (uintptr_t)sp;
}
