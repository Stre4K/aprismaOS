// test.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/printk.h>
#include <string.h>
#include <stdint.h>

void cmd_test(int argc, char **argv)
{
    if (argc < 2)
    {
        printk("Usage: test <thing>\n");
        return;
    }

    if (strcmp(argv[1], "interrupt") == 0)
    {
        printk("Testing interrupt...\n");
        __asm__ volatile("int $0x3");
    }
    else if (strcmp(argv[1], "printk") == 0)
    {
        printk("printk works!\n");
    }
    else if (strcmp(argv[1], "pagefault") == 0)
    {
        volatile uint32_t *ptr = (uint32_t*)0xDEADBEEF;
        *ptr = 0x1234;
    }
    else
    {
        printk("Unknown test\n");
    }
}
