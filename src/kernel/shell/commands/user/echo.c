// echo.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/printk.h>

void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        printk("%s ", argv[i]);
    }
    printk("\n");
}


