#include <kernel/arch/timer.h>
#include <kernel/arch/portio.h>
#include <kernel/arch/pic.h>
#include <kernel/arch/isr.h>
#include <kernel/printk.h>
#include <stdint.h>


#define PIT_CH0 0x40
#define PIT_CH1 0x41
#define PIT_CH2 0x42
#define PIT_CONTROL 0x43 // Mode/Command register (write only, a read is ignored)

#define PIT_MASK 0xFF
#define PIT_SCALE 1193182
#define PIT_SET_RATE_GEN 0x34
#define PIT_SET_SQUARE_WAVE 0x36


#define PIT_IRQ 0

/*
    * The Mode/Command register at I/O address 0x43 contains the following:

Bits         Usage
7 and 6      Select channel :
                0 0 = Channel 0
                0 1 = Channel 1
                1 0 = Channel 2
                1 1 = Read-back command (8254 only)
5 and 4      Access mode :
                0 0 = Latch count value command
                0 1 = Access mode: lobyte only
                1 0 = Access mode: hibyte only
                1 1 = Access mode: lobyte/hibyte
3 to 1       Operating mode :
                0 0 0 = Mode 0 (interrupt on terminal count)
                0 0 1 = Mode 1 (hardware re-triggerable one-shot)
                0 1 0 = Mode 2 (rate generator)
                0 1 1 = Mode 3 (square wave generator)
                1 0 0 = Mode 4 (software triggered strobe)
                1 0 1 = Mode 5 (hardware triggered strobe)
                1 1 0 = Mode 2 (rate generator, same as 010b)
                1 1 1 = Mode 3 (square wave generator, same as 011b)
0            BCD/Binary mode: 0 = 16-bit binary, 1 = four-digit BCD

*/

/* ========================================
    * PIT set frequency
 * ======================================== */

void pit_set_frequency(uint32_t hz) {
    if (hz == 0) {
        return;
    }
    uint16_t divisor = PIT_SCALE / hz;
    outb(PIT_CONTROL, PIT_SET_RATE_GEN);             // channel 0, lobyte/hibyte, mode 2
    outb(PIT_CH0, divisor & PIT_MASK);   // low byte
    outb(PIT_CH0, (divisor >> 8) & PIT_MASK); // high byte
}


/* ========================================
    * PIT interrupt handler
 * ======================================== */

volatile uint64_t pit_ticks = 0;
volatile uint64_t pit_timer = 0;
void pit_interrupt_handler(regs_t* regs) {
    (void)regs;
    pit_ticks++;
    if (pit_ticks % 100 == 0) {
       pit_timer++;
       printk("PIT timer: %lld\n", pit_timer);
    }
    pic_send_eoi(PIT_IRQ);
}

/* ========================================
    * PIT initialization
 * ======================================== */


void pit_init(void) {
    // enable PIT
    register_irq_handler(PIT_IRQ, pit_interrupt_handler);

    // set frequency
    pit_set_frequency(100); // 100 Hz

    // Clear mask for irq0 on PIC
    pic_clear_mask_irq(PIT_IRQ);
}


/* ========================================
 * PIT calibration function - used for lapic
 * ======================================== */

volatile uint32_t count = 0;
void __pit_calibration_handler(regs_t* regs) {
    (void)regs;
    count++;
    pic_send_eoi(PIT_IRQ);
}

/*
    * @desc: Wait for N PIT ticks
    * @param: ticks - number of PIT ticks to wait for (1 tick = 10ms)
    * @return: void
*/
void pit_calibration_wait(uint32_t ticks) {
    disable_interrupts();

    pic_remap(0x20, 0x28);

    register_irq_handler(PIT_IRQ, __pit_calibration_handler);
    pic_clear_mask_irq(PIT_IRQ);
    pit_set_frequency(100); // 100Hz = 10ms per tick

    enable_interrupts();

    // wait for N PIT ticks
    while (count < ticks) {
        asm volatile("hlt");
    }

    disable_interrupts();

    unregister_irq_handler(PIT_IRQ);
    pic_set_mask_irq(PIT_IRQ);
}

