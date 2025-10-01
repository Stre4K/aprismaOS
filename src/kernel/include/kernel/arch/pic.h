#ifndef __KERNEL_ARCH_PIC_H__
#define __KERNEL_ARCH_PIC_H__

#include <stdint.h>

void pic_disable(void);
void pic_send_eoi(uint8_t irq);
void pic_remap(int offset1, int offset2);

#endif
