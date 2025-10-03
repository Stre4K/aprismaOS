#include <kernel/arch/timer.h>
#include <kernel/arch/lapic.h>
#include <kernel/arch/isr.h>
#include <kernel/printk.h>

/* =======================================================
 * LAPIC timer configuration
 * ======================================================= */

static void lapic_timer_configure(uint32_t divide, uint32_t lvt, uint32_t init) {
    lapic_write(LAPIC_TIMER_DIV, divide); // divide by 16 = 0x3
    lapic_write(LAPIC_LVT_TIMER, lvt); // masked (no interrupt)
    lapic_write(LAPIC_TIMER_INIT, init); // Countdown
}

/* =======================================================
 * LAPIC timer initialization
 * ======================================================= */

void lapic_timer_init(uint32_t hz, uint32_t vector) {
    uint32_t ticks_per_second = lapic_timer_calibrate();
    uint32_t ticks_per_period = ticks_per_second / hz;

    // Register handler for IRQ 0 (timer) BEFORE enabling interrupts
    register_irq_handler(0, &timer_interrupt_handler);

    // Divide configuration
    lapic_timer_configure(
        0x3, // divide by 16
        vector | LAPIC_TIMER_MODE_PERIODIC, // vector number
        ticks_per_period // start timer
    );
}

/* =======================================================
    * LAPIC timer helper functions
 * ======================================================= */

uint32_t lapic_timer_read(void) {
    return lapic_read(LAPIC_TIMER_CURR);
}

/* =======================================================
    * LAPIC calibrate timer functions
 * ======================================================= */

void lapic_timer_start_max(void) {
    // 0x3 = divide by 16, 0x10000 = masked, 0xFFFFFFFF = start
    lapic_timer_configure(0x3, 0x10000, 0xFFFFFFFF);
}

uint32_t lapic_timer_calibrate(void) {
    lapic_timer_start_max();
    pit_calibration_wait(5);
    uint32_t remaining = lapic_timer_read();
    uint32_t elapsed = 0xFFFFFFFF - remaining;
    uint32_t ticks_per_millisecond = elapsed / 50;
    uint32_t ticks_per_second = ticks_per_millisecond * 1000;
    return ticks_per_second;
}

/* =======================================================
    * LAPIC timer handler
 * ======================================================= */

volatile uint32_t timer_ticks = 0;
void timer_interrupt_handler(regs_t* regs) {
    (void)regs;
    printk("Timer: %lld\n", timer_ticks++);
    lapic_eoi(); // signal end of interrupt
}

