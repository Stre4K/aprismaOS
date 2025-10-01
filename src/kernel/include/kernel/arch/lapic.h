// arch/i386/include/arch/lapic.h
#ifndef __KERNEL_ARCH_LAPIC_H__
#define __KERNEL_ARCH_LAPIC_H__

#include <stdint.h>

#define LAPIC_BASE_MSR 0x1B
#define LAPIC_ENABLE   (1 << 11)

// LAPIC Registers (offsets from LAPIC base)
#define LAPIC_ID          0x020
#define LAPIC_VERSION     0x030
#define LAPIC_TPR         0x080
#define LAPIC_EOI         0x0B0
#define LAPIC_SVR         0x0F0
#define LAPIC_ICR_LOW     0x300
#define LAPIC_ICR_HIGH    0x310
#define LAPIC_LVT_TIMER   0x320
#define LAPIC_TIMER_INIT  0x380
#define LAPIC_TIMER_CURR  0x390
#define LAPIC_TIMER_DIV    0x3E0

#define LAPIC_TIMER_MODE_PERIODIC (1 << 17)

extern volatile uint32_t *lapic_base;

void lapic_init(void);
void lapic_eoi(void);

static inline void lapic_write(uint32_t reg, uint32_t val) {
    lapic_base[reg / 4] = val;
}

static inline uint32_t lapic_read(uint32_t reg) {
    return lapic_base[reg / 4];
}

#endif
