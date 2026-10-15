// gdt.c (x86_64)

#include <kernel/arch/gdt.h>
#include <kernel/arch/tss.h>
#include <stdint.h>
#include <string.h>

extern void gdt_flush(struct gdt_ptr_struct *ptr);
extern uint8_t stack_top[];

static struct gdt_entry_struct gdt_entries[GDT_ENTRY_COUNT] __attribute__((aligned(16)));
static struct gdt_ptr_struct gdt_ptr;

struct tss_entry_struct tss_entry;

// Separate stack for double faults, so a kernel stack overflow can still be reported
static uint8_t double_fault_stack[8192] __attribute__((aligned(16)));

// Code/data descriptor. Base and limit are ignored in long mode but are set
// to a flat 4 GiB segment like the boot GDT.
static void set_gdt_gate(int num, uint8_t access, uint8_t flags)
{
    gdt_entries[num].limit = 0xFFFF;
    gdt_entries[num].base_low = 0;
    gdt_entries[num].base_middle = 0;
    gdt_entries[num].access = access;
    gdt_entries[num].flags = (flags & 0xF0) | 0x0F;
    gdt_entries[num].base_high = 0;
}

// The 64-bit TSS descriptor is 16 bytes: a normal descriptor followed by
// the upper 32 bits of the base and a reserved dword
static void set_tss_gate(int num, uint64_t base, uint32_t limit)
{
    gdt_entries[num].limit = limit & 0xFFFF;
    gdt_entries[num].base_low = base & 0xFFFF;
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].access = GDT_ACCESS_TSS;
    gdt_entries[num].flags = (limit >> 16) & 0x0F;
    gdt_entries[num].base_high = (base >> 24) & 0xFF;

    uint32_t upper[2] = { (uint32_t)(base >> 32), 0 };
    memcpy(&gdt_entries[num + 1], upper, sizeof(upper));
}

void init_tss(void)
{
    memset(&tss_entry, 0, sizeof(tss_entry));

    tss_entry.rsp0 = (uint64_t)(uintptr_t)stack_top;
    tss_entry.ist[TSS_IST_DOUBLE_FAULT - 1] =
        (uint64_t)(uintptr_t)(double_fault_stack + sizeof(double_fault_stack));

    // No I/O permission bitmap
    tss_entry.iomap_base = sizeof(tss_entry);
}

void write_tss_rsp0(uint64_t rsp0)
{
    tss_entry.rsp0 = rsp0;
}

void init_gdt(void)
{
    memset(gdt_entries, 0, sizeof(gdt_entries));

    set_gdt_gate(GDT_KERNEL_CODE, GDT_ACCESS_CODE_PL0, GDT_FLAGS_CODE64);
    set_gdt_gate(GDT_KERNEL_DATA, GDT_ACCESS_DATA_PL0, GDT_FLAGS_DATA);
    set_gdt_gate(GDT_USER_DATA,   GDT_ACCESS_DATA_PL3, GDT_FLAGS_DATA);
    set_gdt_gate(GDT_USER_CODE,   GDT_ACCESS_CODE_PL3, GDT_FLAGS_CODE64);

    init_tss();
    set_tss_gate(GDT_TSS, (uint64_t)(uintptr_t)&tss_entry, sizeof(tss_entry) - 1);

    gdt_ptr.limit = sizeof(gdt_entries) - 1;
    gdt_ptr.base = (uint64_t)(uintptr_t)gdt_entries;

    gdt_flush(&gdt_ptr);
    load_tss(SEL(GDT_TSS));
}
