// cpumode.c
// <kernel/arch/cpu.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/arch/cpu.h>
#include <kernel/arch/msr.h>
#include <stdint.h>

cpu_mode_t cpu_get_mode(void) {
    uint32_t cr0 = read_cr0();

    if (!(cr0 & 1))
        return CPU_MODE_REAL;        // PE = 0 → Real mode

    // Protected mode (PE=1)
    uint64_t efer = read_msr(0xC0000080); // IA32_EFER MSR
    if (efer & (1 << 10))                  // LMA = Long Mode Active
        return CPU_MODE_LONG;

    return CPU_MODE_PROTECTED;
}

const char* cpu_mode_str(cpu_mode_t mode) {
    switch (mode) {
        case CPU_MODE_REAL: return "Real Mode";
        case CPU_MODE_PROTECTED: return "Protected Mode";
        case CPU_MODE_LONG: return "Long Mode";
        default: return "Unknown";
    }
}

int cpu_is_paging_enabled(void) {
    uint32_t cr0 = read_cr0();
    return (cr0 & (1 << 31)) != 0; // PG bit
}

