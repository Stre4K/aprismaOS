#ifndef __KERNEL_ARCH_TIMER_H__
#define __KERNEL_ARCH_TIMER_H__

#include <stdint.h>

// LAPIC Timer
void timer_interrupt_handler(void* ctx);
void lapic_timer_init(uint32_t initial_count, uint32_t vector);

#endif


