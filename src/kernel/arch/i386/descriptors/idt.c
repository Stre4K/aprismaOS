#include <kernel/arch/idt.h>
#include <kernel/printk.h>
#include <kernel/arch/gdt.h>  // for selector constant if needed
#include <stdint.h>


/* ======================
 * Assembly stubs
 * ====================== */
extern void idt_load(idt_ptr_t*); // assembly wrapper (lidt)
extern void (*isr_stub_table[])(void); // strong reference to all stubs defined in isr_asm.S
    // isr_stub_table is an external symbol ensuring the stubs are linked
    // set gates for CPU exceptions (0..31) and IRQs (32..47) - stubs declared in isr.S
    // We'll set all entries to point to the generated stubs labels isr0..isr47

/* ======================
 * IDT Allocation
 * ====================== */
__attribute__((aligned(0x10)))
static idt_entry_t idt[IDT_ENTRIES]; // Aligned for performance
static idt_ptr_t   idt_ptr;

/* ======================
 * Set IDT Descriptor 32-bit
 * ====================== */
static void set_idt_gate(uint8_t vector, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[vector].isr_low   = base & 0xFFFF;
    idt[vector].kernel_cs = selector;
    idt[vector].reserved  = 0;
    idt[vector].attributes = flags;
    idt[vector].isr_high = (base >> 16) & 0xFFFF;
}
/* ======================
 * Set IDT Descriptor 64-bit
 * ====================== */
/*
void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];

    descriptor->isr_low        = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs      = GDT_OFFSET_KERNEL_CODE;
    descriptor->ist            = 0;
    descriptor->attributes     = flags;
    descriptor->isr_mid        = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high       = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved       = 0;
}
*/

/* ======================
 * Initialize IDT
 * ====================== */

void init_idt(void) {
    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base  = (uintptr_t)&idt;

    // Zero table
    for (int i = 0; i < IDT_ENTRIES; ++i) {
        set_idt_gate(i, 0, 0, 0);
    }

    // Set IDT entries
    for (uint8_t vector = 0; vector < IDT_CURRENT_INTERRUPTS; ++vector) {
        set_idt_gate(vector, (uintptr_t)isr_stub_table[vector], SEL(GDT_KERNEL_CODE), IDT_KERNEL_INT_GATE);
    }

    // Set IDT entries - trap gates
    set_idt_gate(3, (uintptr_t)isr_stub_table[3],  SEL(GDT_KERNEL_CODE), IDT_KERNEL_TRAP_GATE); //TRAP
    set_idt_gate(4, (uintptr_t)isr_stub_table[4],  SEL(GDT_KERNEL_CODE), IDT_KERNEL_TRAP_GATE); //TRAP
    //
       // load the idt
    idt_load(&idt_ptr);
}

