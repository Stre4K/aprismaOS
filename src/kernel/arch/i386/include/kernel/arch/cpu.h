// arch/cpu.h

#ifndef __KERNEL_ARCH_CPU_H_
#define __KERNEL_ARCH_CPU_H_


#include <stdint.h>



typedef enum {
    CPU_MODE_REAL,
    CPU_MODE_PROTECTED,
    CPU_MODE_LONG
} cpu_mode_t;

// Returns the currently active CPU mode
cpu_mode_t cpu_get_mode(void);

// Optional helper: returns string
const char* cpu_mode_str(cpu_mode_t mode);

int cpu_is_paging_enabled(void);

static inline int cpu_get_cpl(void) {
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    return cs & 0x3; // CPL is low 2 bits of CS
}

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
