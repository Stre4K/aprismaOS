// arch/cpu.h

#ifndef __KERNEL_ARCH_CPU_H_
#define __KERNEL_ARCH_CPU_H_


#include <stdint.h>



// CPU mode queries live in <kernel/arch/cpumode.h>

static inline uint32_t read_cr0(void) {
    uint32_t val;
    __asm__ volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint32_t val) {
    __asm__ volatile("mov %0, %%cr0" :: "r"(val));
}

static inline uint32_t read_cr2(void) {
    uint32_t val;
    __asm__ volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

static inline uint32_t read_cr3(void) {
    uint32_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(uint32_t val) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(val));
}

static inline uint32_t read_cr4(void) {
    uint32_t val;
    __asm__ volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(uint32_t val) {
    __asm__ volatile("mov %0, %%cr4" :: "r"(val));
}


#endif
