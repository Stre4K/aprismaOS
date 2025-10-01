#include <kernel/arch/isr.h>
#include <kernel/printk.h>
#include <stdint.h>

/* simple IRQ handler registration */
#define MAX_IRQS 16
static void (*irq_handlers[MAX_IRQS])(void* ctx);

void register_irq_handler(int irq, void (*handler)(void* ctx)) {
    if (irq < 0 || irq >= MAX_IRQS) return;
    irq_handlers[irq] = handler;
}
void unregister_irq_handler(int irq) {
    if (irq < 0 || irq >= MAX_IRQS) return;
    irq_handlers[irq] = 0;
}

/* Called from assembly stubs */
void isr_common_handler(void* regs) {
    regs_t* r = (regs_t*)regs;
    uint8_t int_no = r->int_no;
    printk("[ISR] int_no=%d eax=%x ebx=%x\n", int_no, r->eax, r->ebx);

    if (int_no >= 32 && int_no < 48) {
        int irq = int_no - 32;
        if (irq < MAX_IRQS && irq_handlers[irq]) {
            irq_handlers[irq](regs);
        } else {
            // spurious: just send EOI if necessary in irq handler region (we do that in PIC code)
        }
    } else {
        printk("Unhandled interrupt: %d\n", int_no);
        // Optionally halt or try to continue
        for(;;)
            asm volatile("hlt");
    }
}

void enable_interrupts(void) {
    asm volatile("sti");
}
void disable_interrupts(void) {
    asm volatile("cli");
}


