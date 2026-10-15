// idt.h (x86_64)

#ifndef _KERNEL_IDT_H
#define _KERNEL_IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256
#define IDT_CURRENT_INTERRUPTS 48

#define IDT_FLAG_PRESENT 0x80
#define IDT_FLAG_RING0   0x00
#define IDT_FLAG_RING3   0x60
#define IDT_TYPE_INT64   0x0E  // 64-bit interrupt gate (clears IF)
#define IDT_TYPE_TRAP64  0x0F  // 64-bit trap gate (keeps IF)

#define IDT_KERNEL_INT_GATE  (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_TYPE_INT64)
#define IDT_KERNEL_TRAP_GATE (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_TYPE_TRAP64)

typedef struct {
    uint16_t isr_low;     // handler address bits 0..15
    uint16_t kernel_cs;   // code segment selector
    uint8_t  ist;         // Interrupt Stack Table slot (0 = don't switch)
    uint8_t  attributes;  // type and attributes
    uint16_t isr_mid;     // handler address bits 16..31
    uint32_t isr_high;    // handler address bits 32..63
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

void init_idt(void);

#endif
