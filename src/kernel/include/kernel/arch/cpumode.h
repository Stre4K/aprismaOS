// arch/cpumode.h
// Shared by i386 and x86_64

#ifndef __KERNEL_ARCH_CPUMODE_H_
#define __KERNEL_ARCH_CPUMODE_H_

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

#endif
