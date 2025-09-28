#ifndef _KERNEL_GDT_H
#define _KERNEL_GDT_H
#include <stdint.h>

/*
 * ===============================
 * Segment Descriptor Overview
 * ===============================
 *
 * Each entry in the Global Descriptor Table (GDT) has a complex structure
 * that defines memory segments for the CPU. A 32-bit segment descriptor looks like:
 *
 *  63      56  55      52  51      48  47      40  39      32
 *  +--------+--------+--------+--------+--------+--------+
 *  |        Base High (31:24)       | Flags  |Access  |
 *  +--------+--------+--------+--------+--------+--------+
 *  31      24  23      16  15       0
 *  +--------+--------+--------+--------+
 *  | Base Mid (23:16) | Limit High |    |
 *  +--------+--------+--------+--------+
 *  15      0
 *  +----------------+
 *  | Base Low | Limit Low |
 *  +----------------+
 *
 * Fields:
 * - Base: 32-bit linear address where the segment begins.
 * - Limit: 20-bit maximum addressable unit. If G=1 (page granularity), units are 4 KiB.
 *          Otherwise, units are bytes. In 64-bit mode, Base and Limit are ignored.
 * - Access Byte (low 8 bits of descriptor):
 *      7  6  5  4  3  2  1  0
 *      P  DPL S  E  DC RW A
 *      P   : Present bit. Must be 1 for a valid segment.
 *      DPL : Descriptor Privilege Level. 0=kernel, 3=user.
 *      S   : Descriptor type. 0=system, 1=code/data.
 *      E   : Executable. 0=data, 1=code.
 *      DC  : Direction (data)/Conforming (code) bit.
 *      RW  : Writable (data)/Readable (code) bit.
 *      A   : Accessed bit. CPU sets when segment is accessed.
 *
 * - Flags (upper nibble of next byte, low nibble comes from Limit[19:16]):
 *      3  2  1  0
 *      G  DB L  Reserved
 *      G : Granularity. 0=byte, 1=4KiB pages.
 *      DB: Default operation size. 0=16-bit, 1=32-bit.
 *      L : Long-mode code. 1=64-bit code segment. DB must be 0.
 *      Reserved: Must be 0.
 *
 * Notes:
 * - The CPU uses the Access byte to check privilege and access permissions.
 * - The Flags field modifies how the Limit is interpreted and the segment type.
 * - For full reference, see Intel SDM Vol 3A, Section 3.4.5 and Figure 3-8.
 */


/*
 * ===============================
 * GDT Segment Descriptor Layout
 * ===============================
 *
 * 31                                 0
 * +----------------------------------+
 * |            Base 31:24            | <- gdt_entry.base_high
 * +---+---+---+---+---+---+---+---+
 * |G|DB|L|AVL| Limit 19:16 |Access| <- gdt_entry.flags + gdt_entry.access
 * +---+---+---+---+---+---+---+---+
 * |          Base 23:16             | <- gdt_entry.base_middle
 * +----------------------------------+
 * |          Limit 15:0             | <- gdt_entry.limit
 * +----------------------------------+
 * |          Base 15:0              | <- gdt_entry.base_low
 * +----------------------------------+
 *
 * -------------------------------
 * Flags / Upper Nibble of 2nd Byte:
 * -------------------------------
 * Bit 7: G  - Granularity (0=byte, 1=4 KiB)
 * Bit 6: DB - Default operation size (0=16-bit, 1=32-bit)
 * Bit 5: L  - Long mode (1=64-bit code segment, DB must be 0)
 * Bit 4: AVL - Available for system use
 *
 * -------------------------------
 * Access Byte / Lower 8 bits:
 * -------------------------------
 * Bit 7: P  - Present
 * Bit 6-5: DPL - Descriptor Privilege Level (0-3)
 * Bit 4: S  - Descriptor type (0=system, 1=code/data)
 * Bit 3: E  - Executable (1=code, 0=data)
 * Bit 2: DC - Direction/Conforming (Data: down segment, Code: conforming)
 * Bit 1: RW - Readable (code)/Writable (data)
 * Bit 0: A  - Accessed (CPU sets when segment is accessed)
 *
 * Example Macros:
 * -------------------------------
 * Access byte only (low 8 bits):
 *   #define GDT_ACCESS_CODE_PL0  (SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_PRIV(0) | SEG_CODE_EXRD)
 *   #define GDT_ACCESS_DATA_PL0  (SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_PRIV(0) | SEG_DATA_RDWR)
 *
 * Flags byte (upper nibble; low nibble comes from limit[19:16]):
 *   #define GDT_FLAGS_32BIT_4K  (SEG_GRAN(1) | SEG_SIZE(1) | SEG_LONG(0) | SEG_SAVL(0))
 */

// ------------------------
// Segment types - Used for gdt_entry order
// ------------------------
#define GDT_ENTRY_COUNT 6

#define GDT_NULL        0
#define GDT_KERNEL_CODE 1
#define GDT_KERNEL_DATA 2
#define GDT_USER_CODE   3
#define GDT_USER_DATA   4
#define GDT_TSS         5

// ------------------------
// Segment selectors - Used to get address of segment
// ------------------------
#define SEL(idx) ((idx) * 8) // 8 bytes per entry.
// Example: 0x0000:SEL(GDT_KERNEL_CODE)


// ------------------------
// ACCESS BYTE (low 8 bits)
// ------------------------A
#define SEG_PRES(x)      ((x) << 7) // P: Present
#define SEG_PRIV(x)      (((x) & 0x03) << 5) //  DPL: Descriptor privilege level (0-3)
#define SEG_DESCTYPE(x)  ((x) << 4)      // S: Descriptor type: 0=system, 1=code/data
// Use following bits or combine with enum gdt_access_t
#define SEG_CODE_EX(x)   ((x) << 3) // E/DC: Executable/Direction-Conforming
#define SEG_RW(x)        ((x) << 1) // RW: Readable/Writable
#define SEG_ACCESSED(x)  ((x) << 0) // A: Accessed

typedef enum {
    SEG_DATA_RD        = 0x00, // Read-Only
    SEG_DATA_RDA       = 0x01, // Read-Only, accessed
    SEG_DATA_RDWR      = 0x02, // Read/Write
    SEG_DATA_RDWRA     = 0x03, // Read/Write, accessed
    SEG_DATA_RDEXPD    = 0x04, // Read-Only, expand-down
    SEG_DATA_RDEXPDA   = 0x05, // Read-Only, expand-down, accessed
    SEG_DATA_RDWREXPD  = 0x06, // Read/Write, expand-down
    SEG_DATA_RDWREXPDA = 0x07, // Read/Write, expand-down, accessed
    SEG_CODE_EX        = 0x08, // Execute-Only
    SEG_CODE_EXA       = 0x09, // Execute-Only, accessed
    SEG_CODE_EXRD      = 0x0A, // Execute/Read
    SEG_CODE_EXRDA     = 0x0B, // Execute/Read, accessed
    SEG_CODE_EXC       = 0x0C, // Execute-Only, conforming
    SEG_CODE_EXCA      = 0x0D, // Execute-Only, conforming, accessed
    SEG_CODE_EXRDC     = 0x0E, // Execute/Read, conforming
    SEG_CODE_EXRDCA    = 0x0F // Execute/Read, conforming, accessed
} gdt_access_t;

// ------------------------
// FLAGS BYTE (upper nibble only)
// ------------------------
#define SEG_GRAN(x)      ((x) << 0x07) // G: Granularity (0 for 1B - 1MB, 1 for 4KB - 4GB)
#define SEG_SIZE(x)      ((x) << 0x06) // DB: Size (0 for 16-bit, 1 for 32)
#define SEG_LONG(x)      ((x) << 0x05) // L: Long mode
#define SEG_SAVL(x)      ((x) << 0x04) // AVL: Available for system use


// ------------------------
// Packed macros
// ------------------------

// Access byte only (low 8 bits)
#define GDT_ACCESS_NULL     (0x00)
#define GDT_ACCESS_CODE_PL0 (SEG_PRES(1) | SEG_PRIV(0) | SEG_DESCTYPE(1) | SEG_CODE_EXRD)
#define GDT_ACCESS_DATA_PL0 (SEG_PRES(1) | SEG_PRIV(0) | SEG_DESCTYPE(1) | SEG_DATA_RDWR)
#define GDT_ACCESS_CODE_PL3 (SEG_PRES(1) | SEG_PRIV(3) | SEG_DESCTYPE(1) | SEG_CODE_EXRD)
#define GDT_ACCESS_DATA_PL3 (SEG_PRES(1) | SEG_PRIV(3) | SEG_DESCTYPE(1) | SEG_DATA_RDWR)
#define GDT_ACCESS_TSS      (SEG_PRES(1) | SEG_PRIV(0) | SEG_DESCTYPE(0) | 0x09) // TSS Access Byte: 0x9

// Flags byte (upper nibble; low nibble comes from limit[19:16])
#define GDT_FLAGS_NULL      (0x00)
#define GDT_FLAGS_32BIT_4K  (SEG_GRAN(1) | SEG_SIZE(1) | SEG_LONG(0) | SEG_SAVL(0))
#define GDT_FLAGS_TSS (SEG_GRAN(0) | SEG_SIZE(1) | SEG_LONG(0) | SEG_SAVL(0))


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
    uint32_t base;
} __attribute__((packed));


void init_gdt();
void set_gdt_gate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags);



// Old Deprecated way of creating a descriptor
/*
static inline uint64_t create_descriptor(uint32_t base, uint32_t limit, uint16_t flag) {
    uint64_t descriptor;

    // Create the high 32 bit segment
    descriptor = limit & 0x000F0000;  // set limit bits 19:16
    descriptor |= (flag << 8) & 0x00F0FF00;  // set type, p, dpl, s, g, d/b, l and avl fields
    descriptor |= (base >> 16) & 0x000000FF;  // set base bits 23:16
    descriptor |= base & 0xFF000000;          // set base bits 31:24

    // Shift by 32 to allow for low part of segment
    descriptor <<= 32;

    // Create the low 32 bit segment
    descriptor |= base << 16;          // set base bits 15:0
    descriptor |= limit & 0x0000FFFF;  // set limit bits 15:0

    return descriptor;
}
*/

// Each define here is for a specific flag in the descriptor.
// Refer to the intel documentation for a description of what each one does.
/*
#define SEG_DESCTYPE(x)  ((x) << 0x04) // Descriptor type (0 for system, 1 for code/data)
#define SEG_PRES(x)      ((x) << 0x07) // Present
#define SEG_SAVL(x)      ((x) << 0x0C) // Available for system use
#define SEG_LONG(x)      ((x) << 0x0D) // Long mode
#define SEG_SIZE(x)      ((x) << 0x0E) // Size (0 for 16-bit, 1 for 32)
#define SEG_GRAN(x)      ((x) << 0x0F) // Granularity (0 for 1B - 1MB, 1 for 4KB - 4GB)
#define SEG_PRIV(x)     (((x) &  0x03) << 0x05)   // Set privilege level (0 - 3)
*/


/*
#define GDT_CODE_PL0 SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | \
                     SEG_LONG(0)     | SEG_SIZE(1) | SEG_GRAN(1) | \
                     SEG_PRIV(0)     | SEG_CODE_EXRD

#define GDT_DATA_PL0 SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | \
                     SEG_LONG(0)     | SEG_SIZE(1) | SEG_GRAN(1) | \
                     SEG_PRIV(0)     | SEG_DATA_RDWR

#define GDT_CODE_PL3 SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | \
                     SEG_LONG(0)     | SEG_SIZE(1) | SEG_GRAN(1) | \
                     SEG_PRIV(3)     | SEG_CODE_EXRD

#define GDT_DATA_PL3 SEG_DESCTYPE(1) | SEG_PRES(1) | SEG_SAVL(0) | \
                     SEG_LONG(0)     | SEG_SIZE(1) | SEG_GRAN(1) | \
                     SEG_PRIV(3)     | SEG_DATA_RDWR

#define GDT_NULL SEG_DESCTYPE(0) | SEG_PRES(0) | SEG_SAVL(0) | \
                 SEG_LONG(0)     | SEG_SIZE(1) | SEG_GRAN(1)
*/

#endif
