// stack.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-14


#include <kernel/config.h>

#ifdef CONFIG_CMD_DEBUG

#include <kernel/shell/commands.h>


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

SHELL_CMD("debug", "stack", "Display stack information", cmd_stack);

#endif // CONFIG_CMD_DEBUG
