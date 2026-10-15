// fault.c
// Author: Stre4k
// Date: 2026-03-08

#include <kernel/arch/isr.h>
#include <kernel/arch/cpu.h>
#include <kernel/printk.h>
#include <kernel/panic.h>
#include <stdint.h>

#define PF_PRESENT (1 << 0)  // 0 = page not present, 1 = protection violation
#define PF_WRITE   (1 << 1)
#define PF_USER    (1 << 2)
#define PF_RSVD    (1 << 3)  // reserved bit set in a page table entry
#define PF_FETCH   (1 << 4)  // instruction fetch

void page_fault_handler(regs_t *r)
{
    uintptr_t fault_addr = read_cr2();

    printk("PAGE FAULT at %p: %s %s in %s mode\n", (void *)fault_addr,
           (r->err_code & PF_PRESENT) ? "protection violation on" : "not-present page on",
           (r->err_code & PF_FETCH) ? "instruction fetch" : (r->err_code & PF_WRITE) ? "write" : "read",
           (r->err_code & PF_USER) ? "user" : "kernel");

    if (r->err_code & PF_RSVD)
        printk("reserved bit set in a page table entry\n");

    regs_dump(r);
    panic("page fault");
}
