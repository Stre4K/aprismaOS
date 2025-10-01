#include <kernel/arch/tss.h>
#include <kernel/arch/gdt.h>

struct tss_entry_struct tss_entry;

void init_tss() {
    // Clear the TSS
    for (unsigned long i = 0; i < sizeof(tss_entry); i++)
        ((uint8_t*)&tss_entry)[i] = 0;

    extern uint32_t stack_top;
    // Set SS0/ESP0 for kernel stack
    tss_entry.esp0 = (uintptr_t)&stack_top;                 // Set your kernel stack pointer
    tss_entry.ss0 = SEL(GDT_KERNEL_DATA);  // e.g., 0x10

    // Disable I/O bitmap
    tss_entry.iomap_base = (uint16_t)sizeof(struct tss_entry_struct);

    // Add TSS descriptor into the GDT
    set_gdt_gate(GDT_TSS, (uintptr_t)&tss_entry,
                 sizeof(struct tss_entry_struct)-1,
                 GDT_ACCESS_TSS, GDT_FLAGS_TSS); // Access = 0x89, Flags = 0x40
}

void write_tss(uint16_t ss0, uint32_t esp0) {
    tss_entry.ss0 = ss0;
    tss_entry.esp0 = esp0;
}

