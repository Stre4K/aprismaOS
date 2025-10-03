#include <kernel/arch/timer.h>
#include <kernel/arch/lapic.h>
#include <kernel/arch/isr.h>
#include <kernel/printk.h>

void lapic_timer_init(uint32_t initial_count, uint32_t vector) {
    // Register handler for IRQ 0 (timer) BEFORE enabling interrupts
    register_irq_handler(0, &timer_interrupt_handler);

    // Divide configuration
    lapic_write(LAPIC_TIMER_DIV, 0x3); // divide by 16
    lapic_write(LAPIC_LVT_TIMER, vector | LAPIC_TIMER_MODE_PERIODIC); // vector number
    lapic_write(LAPIC_TIMER_INIT, initial_count); // start timer

}


void timer_interrupt_handler(regs_t* regs) {
    (void)regs;
    printk("Timer tick\n");
    lapic_eoi(); // signal end of interrupt
}
