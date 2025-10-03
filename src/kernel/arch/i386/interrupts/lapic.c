// arch/i386/lapic.c
#include <kernel/arch/lapic.h>
#include <kernel/arch/msr.h> // simple read_msr/write_msr
#include <kernel/arch/portio.h>
#include <kernel/arch/pic.h>

volatile uint32_t *lapic_base = 0;

void lapic_init(void) {

    // Disable Legacy PIC
    pic_disable();

    // Read LAPIC base from MSR, 0x1B
    uint64_t base = read_msr(LAPIC_BASE_MSR);
    lapic_base = (volatile uint32_t *)((uintptr_t)base & 0xFFFFF000);

    // Enable LAPIC
    lapic_write(LAPIC_SVR, 0x100 | 0xFF); // 0x100 = enable, 0xFF = spurious vector
}

void lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

