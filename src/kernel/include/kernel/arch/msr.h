#ifndef __KERNEL_ARCH_MSR_H__
#define __KERNEL_ARCH_MSR_H__

#include <stdint.h>

/**
 * Read a 64-bit MSR (Model Specific Register)
 * @param msr - MSR index
 * @return value of the MSR
 */
static inline uint64_t read_msr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile (
        "rdmsr"
        : "=a"(lo), "=d"(hi)
        : "c"(msr)
    );
    return ((uint64_t)hi << 32) | lo;
}

/**
 * Write a 64-bit MSR
 * @param msr - MSR index
 * @param value - value to write
 */
static inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t lo = value & 0xFFFFFFFF;
    uint32_t hi = value >> 32;
    __asm__ volatile (
        "wrmsr"
        :
        : "c"(msr), "a"(lo), "d"(hi)
    );
}

#endif
