#ifndef __KERNEL_ARCH_TIMER_H__
#define __KERNEL_ARCH_TIMER_H__

#include <kernel/arch/isr.h>
#include <stdint.h>

// LAPIC Timer
void timer_interrupt_handler(regs_t* regs);
void lapic_timer_init(uint32_t hz, uint32_t vector);
uint32_t lapic_timer_read();
uint32_t lapic_timer_calibrate();

// PIT
void pit_set_frequency(uint32_t hz);
void pit_interrupt_handler(regs_t* regs);
void pit_init();
void pit_calibration_wait(uint32_t ticks);

#endif


