#include <kernel/arch/pic.h>
#include <kernel/arch/portio.h>
#include <kernel/printk.h>

#include <stdint.h>

#define PIC1 0x20 // Master
#define PIC1_COMMAND 0x20
#define PIC1_OFFSET 0x20
#define PIC1_DATA (PIC1+1)

#define PIC2 0xA0 // Slave
#define PIC2_COMMAND 0xA0
#define PIC2_OFFSET 0x28
#define PIC2_DATA (PIC2+1)

#define PIC_EOI 0x20 // End of interrupt command code

#define ICW1_ICW4	0x01		/* Indicates that ICW4 will be present */
#define ICW1_SINGLE	0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	0x10		/* Initialization - required! */

#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
#define ICW4_SFNM	0x10		/* Special fully nested (not) */

#define CASCADE_IRQ 2

#define PIC_READ_IRR                0x0a    /* OCW3 irq ready next CMD read */
#define PIC_READ_ISR                0x0b    /* OCW3 irq service next CMD read */


#define PIC_WAIT2() \
  	do { \
   		/* May be fragile */ \
   		asm volatile("jmp 1f\n\t" \
   		             "1:\n\t" \
   		             "    jmp 2f\n\t" \
   		             "2:"); \
  	} while (0)


#define PIC_WAIT() \
    do { asm volatile("outb %%al, $0x80" : : "a"(0)); } while (0)


void pic_disable(void) {
    // mask all interrupts from PIC1 and PIC2
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

/* Remap PIC: master to offset1, slave to offset2 (commonly 0x20,0x28) */
void pic_remap(int offset1, int offset2) {
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

	/* Cascade initialization */
	outb(PIC1_COMMAND, ICW1_INIT|ICW1_ICW4); PIC_WAIT();
	outb(PIC2_COMMAND, ICW1_INIT|ICW1_ICW4); PIC_WAIT();

	/* Remap */
	outb(PIC1_DATA, offset1); PIC_WAIT(); // vector offset for master
	outb(PIC2_DATA, offset2); PIC_WAIT(); // vector offset for slave

	/* Cascade identity with slave PIC at IRQ2 */
	outb(PIC1_DATA, 0x04); PIC_WAIT(); // tell Master there is a slave at IRQ2 / 1 << CASCADE_IRQ
	outb(PIC2_DATA, 0x02); PIC_WAIT(); // tell Slave its cascade identity

	/* Request 8086 mode on each PIC */
	outb(PIC1_DATA, ICW4_8086); PIC_WAIT(); // 8086/88 (MCS-80/85) mode
	outb(PIC2_DATA, ICW4_8086); PIC_WAIT();

    // restore saved masks on both PICs
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

void pic_set_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_clear_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

/* Helper func */
static uint16_t __pic_get_irq_reg(int ocw3)
{
    /* OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
     * represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain */
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/* Returns the combined value of the cascaded PICs irq request register */
uint16_t pic_get_irr(void)
{
    return __pic_get_irq_reg(PIC_READ_IRR);
}

/* Returns the combined value of the cascaded PICs in-service register */
uint16_t pic_get_isr(void)
{
    return __pic_get_irq_reg(PIC_READ_ISR);
}


/*
    * @brief Initialize the PICs, set up remap and mask all IRQs
    * @param void
    * @return void
*/
void pic_init(void) {
    pic_remap(PIC1_OFFSET, PIC2_OFFSET);

    // MASK ALL IRQs
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    printk("PIC masks: master=%02x slave=%02x\n", inb(PIC1_DATA), inb(PIC2_DATA));
}
