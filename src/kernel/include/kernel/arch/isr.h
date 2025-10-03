#ifndef _KERNEL_ISR_H
#define _KERNEL_ISR_H

#include <stdint.h>


typedef struct {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) regs_t;


/* simple IRQ handler registration */
void register_irq_handler(int irq, void (*handler)(regs_t* regs));
void unregister_irq_handler(int irq);
void isr_common_handler(void* regs);
void enable_interrupts(void);
void disable_interrupts(void);

#endif

