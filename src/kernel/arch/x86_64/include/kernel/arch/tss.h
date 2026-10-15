// tss.h (x86_64)

#ifndef _KERNEL_TSS_H
#define _KERNEL_TSS_H

#include <stdint.h>

/*
 * The 64-bit TSS no longer holds task state. It provides the stack used when
 * an interrupt arrives from ring 3 (rsp0) and up to seven Interrupt Stack
 * Table entries, which the IDT can select per vector so critical exceptions
 * (e.g. double fault) always run on a known good stack.
 */
struct tss_entry_struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

// IST slot used for the double fault handler (1-based, as in the IDT)
#define TSS_IST_DOUBLE_FAULT 1

extern struct tss_entry_struct tss_entry;

void init_tss(void);
void write_tss_rsp0(uint64_t rsp0);

static inline void load_tss(uint16_t selector) {
    asm volatile("ltr %0" : : "r"(selector));
}

#endif
