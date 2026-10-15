// fault.c
// Author: Stre4k
// Date: 2026-03-08

#include <kernel/arch/isr.h>
#include <kernel/printk.h>
#include <kernel/panic.h>
#include <stdint.h>


void page_fault_handler(regs_t *r)
{
    uint32_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));

    printk("PAGE FAULT\n");
    printk("addr: %x\n", fault_addr);
    printk("error: %x\n", r->err_code);
    printk("eip: %x\n", r->eip);

    if (!(r->err_code & 1))
        printk("not-present page\n");

    if (r->err_code & 2)
        printk("write fault\n");

    if (r->err_code & 4)
        printk("user-mode fault\n");

    panic("page fault");
}
