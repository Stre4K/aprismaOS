// arch/irq.h
// Interrupt-safe critical sections (x86, shared by i386 and x86_64)
//
//   unsigned long flags = irq_save();
//   ... code that must not be interrupted ...
//   irq_restore(flags);
//
// On a single CPU this is enough to protect data shared with interrupt
// handlers. Sections nest, because each restores the previous state.

#ifndef __KERNEL_ARCH_IRQ_H__
#define __KERNEL_ARCH_IRQ_H__

#include <stdbool.h>

#define EFLAGS_IF (1UL << 9)

// Disable interrupts and return the previous flags register
static inline unsigned long irq_save(void) {
    unsigned long flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

// Restore the interrupt state saved by irq_save()
static inline void irq_restore(unsigned long flags) {
    __asm__ volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
}

static inline bool irqs_enabled(void) {
    unsigned long flags;
    __asm__ volatile("pushf; pop %0" : "=r"(flags));
    return (flags & EFLAGS_IF) != 0;
}

#endif // __KERNEL_ARCH_IRQ_H__
