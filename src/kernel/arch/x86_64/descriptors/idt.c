// idt.c (x86_64)

#include <kernel/arch/idt.h>
#include <kernel/arch/gdt.h>
#include <kernel/arch/tss.h>
#include <stdint.h>
#include <string.h>

extern void *isr_stub_table[]; // defined in isr_asm.S

static idt_entry_t idt[IDT_ENTRIES] __attribute__((aligned(16)));
static idt_ptr_t idt_ptr;

static void set_idt_gate(uint8_t vector, uint64_t handler, uint8_t ist, uint8_t flags)
{
    idt[vector].isr_low = handler & 0xFFFF;
    idt[vector].kernel_cs = SEL(GDT_KERNEL_CODE);
    idt[vector].ist = ist;
    idt[vector].attributes = flags;
    idt[vector].isr_mid = (handler >> 16) & 0xFFFF;
    idt[vector].isr_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

void init_idt(void)
{
    memset(idt, 0, sizeof(idt));

    for (int vector = 0; vector < IDT_CURRENT_INTERRUPTS; vector++)
        set_idt_gate(vector, (uint64_t)(uintptr_t)isr_stub_table[vector], 0, IDT_KERNEL_INT_GATE);

    // Breakpoint and overflow are traps, as on i386
    set_idt_gate(3, (uint64_t)(uintptr_t)isr_stub_table[3], 0, IDT_KERNEL_TRAP_GATE);
    set_idt_gate(4, (uint64_t)(uintptr_t)isr_stub_table[4], 0, IDT_KERNEL_TRAP_GATE);

    // Double fault always runs on its own known-good stack
    set_idt_gate(8, (uint64_t)(uintptr_t)isr_stub_table[8], TSS_IST_DOUBLE_FAULT, IDT_KERNEL_INT_GATE);

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)(uintptr_t)idt;

    asm volatile("lidt %0" :: "m"(idt_ptr) : "memory");
}
