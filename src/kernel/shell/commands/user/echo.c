// echo.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-14

#include <kernel/config.h>

#ifdef CONFIG_CMD_USER

#include <kernel/shell/commands.h>

#include <kernel/printk.h>

void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        printk("%s ", argv[i]);
    }
    printk("\n");
}

SHELL_CMD("user", "echo", "Print text", cmd_echo);

#endif // CONFIG_CMD_USER
