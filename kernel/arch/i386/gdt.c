// Used for creating GDT segment descriptors in 64-bit integer form.

#include <stdio.h>
#include <stdint.h>

#include <kernel/gdt.h>

extern void gdt_flush(uint32_t);

struct gdt_entry_struct gdt_entires[5];
struct gdt_ptr_struct gdt_ptr;

void init_gdt(void) {
    gdt_ptr.limit = sizeof(struct gdt_entry_struct) * 5 - 1;
    gdt_ptr.base = (uint32_t) &gdt_entires;

    set_gdt_gate(0, 0, 0, 0, 0); // NULL SEGMENT
    set_gdt_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // KERNEL CODE SEGMENT
    set_gdt_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // KERNEL DATA SEGMENT
    set_gdt_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // USER CODE SEGMENT
    set_gdt_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // USER DATA SEGMENT

    gdt_flush((uint32_t) &gdt_ptr);
}

void set_gdt_gate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt_entires[num].base_low = base & 0xFFFF;
    gdt_entires[num].base_middle = (base >> 16) & 0xFF;
    gdt_entires[num].base_high = (base >> 24) & 0xFF;

    gdt_entires[num].limit = limit & 0xFFFF;
    gdt_entires[num].access = access;
    gdt_entires[num].flags = (limit >> 16) & 0x0F;
    gdt_entires[num].flags |= (flags & 0xF0);
}

void create_descriptor(uint32_t base, uint32_t limit, uint16_t flag)
{
    uint64_t descriptor;

    // Create the high 32 bit segment
    descriptor  =  limit       & 0x000F0000;         // set limit bits 19:16
    descriptor |= (flag <<  8) & 0x00F0FF00;         // set type, p, dpl, s, g, d/b, l and avl fields
    descriptor |= (base >> 16) & 0x000000FF;         // set base bits 23:16
    descriptor |=  base        & 0xFF000000;         // set base bits 31:24

    // Shift by 32 to allow for low part of segment
    descriptor <<= 32;

    // Create the low 32 bit segment
    descriptor |= base  << 16;                       // set base bits 15:0
    descriptor |= limit  & 0x0000FFFF;               // set limit bits 15:0

    //printf("0x%.16llX\n", descriptor);
}


