// stack.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/printk.h>
#include <stdint.h>

void cmd_stack(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    uint32_t esp;

    __asm__ volatile("mov %%esp, %0" : "=r"(esp));

    printk("Current ESP: 0x%x\n", esp);
}
