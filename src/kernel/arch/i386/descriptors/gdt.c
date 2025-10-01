// Used for creating GDT segment descriptors in 64-bit integer form.

#include <kernel/arch/gdt.h>
#include <kernel/arch/tss.h>
#include <kernel/printk.h>
#include <stdint.h>

extern void gdt_flush(uint32_t);

// GDT as raw 64bit integers.
//uint64_t gdt[GDT_ENTRY_COUNT];

struct gdt_entry_struct gdt_entries[GDT_ENTRY_COUNT];
struct gdt_ptr_struct gdt_ptr;

void init_gdt(void) {
    //gdt_ptr.limit = sizeof(struct gdt_entry_struct) * 5 - 1;
    //gdt_ptr.base = (uint32_t)&gdt_entries;

    gdt_ptr.limit = GDT_ENTRY_COUNT * sizeof(struct gdt_entry_struct) - 1;
    gdt_ptr.base = (uintptr_t)&gdt_entries;


    set_gdt_gate(GDT_NULL, 0, 0, GDT_ACCESS_NULL, GDT_FLAGS_NULL);                 // NULL SEGMENT
    set_gdt_gate(GDT_KERNEL_CODE, 0, 0x000FFFFF, GDT_ACCESS_CODE_PL0, GDT_FLAGS_32BIT_4K);  // KERNEL CODE SEGMENT
    set_gdt_gate(GDT_KERNEL_DATA, 0, 0x000FFFFF, GDT_ACCESS_DATA_PL0, GDT_FLAGS_32BIT_4K);  // KERNEL DATA SEGMENT
    set_gdt_gate(GDT_USER_CODE, 0, 0x000FFFFF, GDT_ACCESS_CODE_PL3, GDT_FLAGS_32BIT_4K);  // USER CODE SEGMENT
    set_gdt_gate(GDT_USER_DATA, 0, 0x000FFFFF, GDT_ACCESS_DATA_PL3, GDT_FLAGS_32BIT_4K);  // USER DATA SEGMENT

    init_tss();

    gdt_flush((uintptr_t)&gdt_ptr);

    load_tss(SEL(GDT_TSS));
    }

void set_gdt_gate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access,
                  uint8_t flags) {
    gdt_entries[num].base_low = base & 0xFFFF;
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high = (base >> 24) & 0xFF;

    gdt_entries[num].limit = limit & 0xFFFF;
    gdt_entries[num].access = access;
    gdt_entries[num].flags = (limit >> 16) & 0x0F;
    gdt_entries[num].flags |= (flags & 0xF0);
}

