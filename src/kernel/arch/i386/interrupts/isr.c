#include <kernel/arch/isr.h>
#include <kernel/arch/lapic.h>
#include <kernel/arch/timer.h>
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

    if (int_no == 32) {
        printk("[ISR] Timer interrupt\n");
        lapic_eoi();
        return;
    }

    if (int_no >= 32 && int_no < 48) {
        int irq = int_no - 32;
        if (irq < MAX_IRQS && irq_handlers[irq]) {
            // call the registered handler; handler MUST call lapic_eoi()/pic_send_eoi()
            irq_handlers[irq](regs);
            return;
        } else {
            // spurious: just send EOI if necessary in irq handler region (we do that in PIC code)
            // no handler registered -> send EOI to avoid blocking further interrupts
            lapic_eoi();
            return;
        }
    }

    printk("Unhandled interrupt: %d\n", int_no);
    // Optionally halt or try to continue
    for(;;)
        asm volatile("hlt");
}

void enable_interrupts(void) {
    asm volatile("sti");
}
void disable_interrupts(void) {
    asm volatile("cli");
}


