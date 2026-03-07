// panic.c
// <kernel/panic.h>
// Author: Stre4k
// Date: 2026-03-07

#include <kernel/printk.h>

void panic(const char *msg)
{
    printk("\n====================\n");
    printk("   KERNEL PANIC\n");
    printk("====================\n");
    printk("%s\n", msg);
    printk("System halted.\n");

    for (;;)
        __asm__ volatile("cli; hlt");
}
