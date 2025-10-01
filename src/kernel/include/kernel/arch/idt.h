#ifndef _KERNEL_IDT_H
#define _KERNEL_IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256
#define IDT_CURRENT_INTERRUPTS 48

/* IDT gate type flags:
 * Bit 7: Present (1 = present)
 * Bits 6–5: Descriptor Privilege Level (0 = kernel, 3 = user)
 * Bit 4: Storage Segment (0 = system segment)
 * Bits 3–0: Gate type (0xE = 32-bit interrupt gate, 0xF = trap gate)
 */
#define IDT_FLAG_PRESENT   0x80  /* Bit 7: Descriptor is present */
#define IDT_FLAG_RING0     0x00  /* Bits 6–5: Ring 0 (kernel) */
#define IDT_FLAG_RING3     0x60  /* Bits 6–5: Ring 3 (user) */
#define IDT_TYPE_INT32     0x0E  /* Bits 3–0: 32-bit interrupt gate */
#define IDT_TYPE_TRAP32    0x0F  /* Bits 3–0: 32-bit trap gate */

/* Common combinations */
#define IDT_KERNEL_INT_GATE  (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_TYPE_INT32)
#define IDT_KERNEL_TRAP_GATE (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_TYPE_TRAP32)
#define IDT_USER_INT_GATE    (IDT_FLAG_PRESENT | IDT_FLAG_RING3 | IDT_TYPE_INT32)
#define IDT_USER_TRAP_GATE   (IDT_FLAG_PRESENT | IDT_FLAG_RING3 | IDT_TYPE_TRAP32)


/* ============================
 * IDT Entry 32-bit format
 * ============================ */
typedef struct {
	uint16_t    isr_low;      // The lower 16 bits of the ISR's address
	uint16_t    kernel_cs;    // The GDT segment selector that the CPU will load into CS before calling the ISR
	uint8_t     reserved;     // Set to zero
	uint8_t     attributes;   // Type and attributes; see the IDT page
	uint16_t    isr_high;     // The higher 16 bits of the ISR's address
} __attribute__((packed)) idt_entry_t;

/* ============================
 * IDT Entry 64-bit format
 * ============================ */
/*
typedef struct {
	uint16_t    isr_low;      // The lower 16 bits of the ISR's address
	uint16_t    kernel_cs;    // The GDT segment selector that the CPU will load into CS before calling the ISR
	uint8_t	    ist;          // The IST in the TSS that the CPU will load into RSP; set to zero for now
	uint8_t     attributes;   // Type and attributes; see the IDT page
	uint16_t    isr_mid;      // The higher 16 bits of the lower 32 bits of the ISR's address
	uint32_t    isr_high;     // The higher 32 bits of the ISR's address
	uint32_t    reserved;     // Set to zero
} __attribute__((packed)) idt_entry_t;
*/

/* ============================
 * IDT Pointer (idtr) 32-bit
 * ============================ */
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

/* ============================
 * IDT Pointer (idtr) 64-bit
 * ============================ */
/*
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;
*/

void init_idt(void);

#endif

