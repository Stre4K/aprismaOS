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

    uintptr_t sp;

#if defined(__x86_64__)
    __asm__ volatile("mov %%rsp, %0" : "=r"(sp));
    printk("Current RSP: %p\n", (void *)sp);
#else
    __asm__ volatile("mov %%esp, %0" : "=r"(sp));
    printk("Current ESP: %p\n", (void *)sp);
#endif
}

SHELL_CMD("debug", "stack", "Display stack information", cmd_stack);

#endif // CONFIG_CMD_DEBUG
