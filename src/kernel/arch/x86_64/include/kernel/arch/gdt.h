// gdt.h (x86_64)

#ifndef _KERNEL_GDT_H
#define _KERNEL_GDT_H

#include <stdint.h>

/*
 * In long mode segmentation is mostly disabled: base and limit are ignored
 * for code/data segments. What still matters is the access byte (present,
 * DPL, code/data) and the L bit, which marks a 64-bit code segment.
 *
 * Entry order follows what SYSCALL/SYSRET expect later on: user data must
 * come directly before user code. The TSS descriptor is 16 bytes in long
 * mode and therefore takes two slots.
 */
#define GDT_NULL        0
#define GDT_KERNEL_CODE 1
#define GDT_KERNEL_DATA 2
#define GDT_USER_DATA   3
#define GDT_USER_CODE   4
#define GDT_TSS         5   // slots 5 and 6
#define GDT_ENTRY_COUNT 7

#define SEL(idx) ((idx) * 8)

// Access byte
#define GDT_ACCESS_CODE_PL0 0x9A  // present, ring 0, code, readable
#define GDT_ACCESS_DATA_PL0 0x92  // present, ring 0, data, writable
#define GDT_ACCESS_CODE_PL3 0xFA  // present, ring 3, code, readable
#define GDT_ACCESS_DATA_PL3 0xF2  // present, ring 3, data, writable
#define GDT_ACCESS_TSS      0x89  // present, ring 0, 64-bit TSS (available)

// Flags nibble (upper 4 bits of the flags byte)
#define GDT_FLAGS_CODE64 0xA0  // G=1, L=1 (D must be 0)
#define GDT_FLAGS_DATA   0xC0  // G=1, D/B=1

struct gdt_entry_struct {
    uint16_t limit;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t flags;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr_struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void init_gdt(void);

#endif
