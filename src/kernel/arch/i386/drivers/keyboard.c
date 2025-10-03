// kernel/arch/i386/drivers/keyboard.c
#include <kernel/drivers/keyboard.h>
#include <kernel/arch/portio.h>
#include <kernel/arch/isr.h>
#include <kernel/arch/pic.h>
#include <kernel/printk.h>
#include <stdint.h>

/* Keyboard I/O ports */
#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

// PIC IRQs vector
#define KBD_IRQ 1

/* Small scancode->ascii table for set 1 (non-shifted). Extend as needed. */
static const char scancode_to_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', /* Enter */
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
    /* remaining mostly 0 */
};


/* ======================================================================
 * Keyboard interrupt handler
 * ====================================================================== */

static void keyboard_irq_handler(regs_t *regs) {
    (void)regs;
    uint8_t sc = inb(KBD_DATA_PORT);
    if (sc & 0x80) {
        /* key release - ignore for now */
    } else {
        char c = 0;
        if (sc < sizeof(scancode_to_ascii))
            c = scancode_to_ascii[sc];
        if (c) {
            printk("%c", c);
        }
    }
    /* Tell PIC we're done */
    pic_send_eoi(KBD_IRQ); // IRQ1
}


/* ======================================================================
 * Keyboard driver Initialization
 * ====================================================================== */

/* Startup helper to register keyboard driver. Call from kernel init after idt/pic setup */
void keyboard_init(void) {
    /* mask/unmask IRQ1 as needed. We'll clear the mask for IRQ1 */
    pic_clear_mask_irq(KBD_IRQ);

    /* register handler for IRQ1 (IRQ number not vector): register for irq index 1 */
    register_irq_handler(KBD_IRQ, keyboard_irq_handler);
}

