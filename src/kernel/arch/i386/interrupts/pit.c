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
#define PIT_SET 0x34
#define PIT_SET_OLD 0x36


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

void pit_set_frequency(uint32_t hz) {
    if (hz == 0) {
        return;
    }
    uint16_t divisor = PIT_SCALE / hz;
    outb(PIT_CONTROL, PIT_SET);             // channel 0, lobyte/hibyte, mode 3
    outb(PIT_CH0, divisor & PIT_MASK);   // low byte
    outb(PIT_CH0, (divisor >> 8) & PIT_MASK); // high byte
}

void pit_interrupt_handler(regs_t* regs) {
    (void)regs;
    pic_send_eoi(PIT_IRQ);
    printk("PIT interrupt HANDLER\n");
}

void pit_init(void) {
    // enable PIT
    register_irq_handler(PIT_IRQ, pit_interrupt_handler);

    // Clear mask for irq0 on PIC
    pic_clear_mask_irq(PIT_IRQ);

    // set frequency
    pit_set_frequency(19);
}
