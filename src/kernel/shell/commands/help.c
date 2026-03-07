// help.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/shell/shell.h>

#include <kernel/printk.h>

void cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;
    const shell_command_t *cmds = shell_get_commands();
    size_t num_commands = shell_get_num_commands();
    printk("Available commands:\n");
    for (size_t i = 0; i < num_commands; i++) {
        printk("%s - %s\n", cmds[i].name, cmds[i].description ? cmds[i].description : "No description");
    }
}


