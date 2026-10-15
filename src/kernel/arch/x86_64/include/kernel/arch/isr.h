// isr.h (x86_64)

#ifndef _KERNEL_ISR_H
#define _KERNEL_ISR_H

#include <stdint.h>

// Interrupt frame as built by isr_asm.S: general purpose registers pushed by
// the stub, then the vector and error code, then what the CPU pushed
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) regs_t;

/* ISR handler registration */
void register_isr_handler(int isr, void (*handler)(regs_t* regs));
void unregister_isr_handler(int isr);

/* simple IRQ handler registration */
void register_irq_handler(int irq, void (*handler)(regs_t* regs));
void unregister_irq_handler(int irq);

void isr_common_handler(void* regs);
void enable_interrupts(void);
void disable_interrupts(void);

/* Print the saved registers of an interrupt frame (arch specific) */
void regs_dump(regs_t *regs);

#endif
