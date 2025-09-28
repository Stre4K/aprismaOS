#ifndef _KERNEL_TSS_H
#define _KERNEL_TSS_H
#include <stdint.h>

struct tss_entry_struct {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

extern struct tss_entry_struct tss_entry;

void init_tss();
void write_tss(uint16_t ss0, uint32_t esp0);

static inline void load_tss(uint16_t selector) {
    asm volatile("ltr %0" : : "r"(selector));
}

#endif

