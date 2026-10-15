#include <kernel/arch/isr.h>
#include <kernel/arch/lapic.h>
#include <kernel/arch/pic.h>
#include <kernel/arch/timer.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#ifdef CONFIG_USERMODE
#include <kernel/arch/cpu.h>
#include <kernel/arch/syscall.h>
#include <kernel/proc.h>
#include <stdio.h>
#endif
#include <stdint.h>

/* ========================================================
 * simple ISR handler registration
 * ======================================================== */
#define MAX_ISRS 32
static void (*isr_handlers[MAX_ISRS])(regs_t* regs);

void register_isr_handler(int isr, void (*handler)(regs_t* regs)) {
    if (isr < 0 || isr >= MAX_ISRS) return;
    isr_handlers[isr] = handler;
}

void unregister_isr_handler(int isr) {
    if (isr < 0 || isr >= MAX_ISRS) return;
    isr_handlers[isr] = 0;
}

/* ========================================================
 * simple IRQ handler registration
 * ======================================================== */

#define MAX_IRQS 16
static void (*irq_handlers[MAX_IRQS])(regs_t* regs);

void register_irq_handler(int irq, void (*handler)(regs_t* regs)) {
    if (irq < 0 || irq >= MAX_IRQS) return;
    irq_handlers[irq] = handler;
}
void unregister_irq_handler(int irq) {
    if (irq < 0 || irq >= MAX_IRQS) return;
    irq_handlers[irq] = 0;
}



static const char *exception_names[32] = {
    "Divide error", "Debug", "Non-maskable interrupt", "Breakpoint",
    "Overflow", "Bound range exceeded", "Invalid opcode", "Device not available",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS", "Segment not present",
    "Stack-segment fault", "General protection fault", "Page fault", "Reserved",
    "x87 floating-point exception", "Alignment check", "Machine check", "SIMD floating-point exception",
    "Virtualization exception", "Control protection exception", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor injection exception", "VMM communication exception", "Security exception", "Reserved",
};

/* ========================================================
 * Interrupt common handler - called from assembly isr_asm.S
 * ======================================================== */

/* Called from assembly stubs */
void isr_common_handler(void* regs) {
    regs_t* r = (regs_t*)regs;
    uint8_t int_no = r->int_no;
    //printk("[ISR] int_no: %d\n", int_no);


#ifdef CONFIG_USERMODE
    /* System calls */
    if (int_no == SYSCALL_VECTOR) {
        syscall_dispatch(r);
        return;
    }

    /* A fault in user mode ends that process, not the kernel */
    if (int_no < 32 && REGS_FROM_USER(r)) {
        char reason[96];
        if (int_no == 14)
            snprintf(reason, sizeof(reason), "%s at address 0x%llx", exception_names[int_no],
                     (unsigned long long)read_cr2());
        else
            snprintf(reason, sizeof(reason), "%s", exception_names[int_no]);
        process_kill(reason, r);
    }
#endif

    /* CPU exceptions */
    if (int_no < 32) {
        if (isr_handlers[int_no]) {
            isr_handlers[int_no](r);
            return;
        }

        printk("Unhandled CPU exception %d: %s\n", int_no, exception_names[int_no]);
        regs_dump(r);
        for(;;) asm volatile("cli; hlt");
    }

    /* Hardware IRQs */
    if (int_no >= 32 && int_no < 48) {
        int irq = int_no - 32;
        if (irq < MAX_IRQS && irq_handlers[irq]) {
            // call the registered handler; handler MUST call lapic_eoi()/pic_send_eoi()
            irq_handlers[irq](regs);
        } else {
            // no handler registered -> send EOI to avoid blocking further interrupts
            pic_send_eoi(irq);
        }
#ifdef CONFIG_SCHED
        // The interrupt is acknowledged; switch threads here if one is due
        sched_irq_exit();
#endif
        return;
    }

    printk("Unhandled interrupt: %d\n", int_no);
    // Optionally halt or try to continue
    for(;;)
        asm volatile("hlt");
}



/* ========================================================
    * Interrupt enable/disable
* ======================================================== */

void enable_interrupts(void) {
    asm volatile("sti" ::: "memory");
}
void disable_interrupts(void) {
    asm volatile("cli" ::: "memory");
}

// Enable interrupts and halt until the next one. sti takes effect only
// after the following instruction, so no interrupt can arrive between the
// two: a caller that checked for work with interrupts disabled cannot miss
// a wakeup.
void enable_interrupts_and_halt(void) {
    asm volatile("sti; hlt" ::: "memory");
}
