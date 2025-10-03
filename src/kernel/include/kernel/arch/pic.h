#ifndef __KERNEL_ARCH_PIC_H__
#define __KERNEL_ARCH_PIC_H__

#include <stdint.h>

void pic_disable(void);
void pic_send_eoi(uint8_t irq);
void pic_remap(int offset1, int offset2);
void pic_set_mask_irq(uint8_t irq);
void pic_clear_mask_irq(uint8_t irq);
uint16_t pic_get_isr(void);
uint16_t pic_get_irr(void);
void pic_init(void);

#endif
