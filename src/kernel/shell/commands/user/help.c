// help.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-14

#include <kernel/config.h>

#ifdef CONFIG_CMD_USER

#include <kernel/shell/shell.h>
#include <kernel/shell/commands.h>

#include <kernel/printk.h>
#include <string.h>

/*
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
*/


extern shell_command_t __start_shell_commands;
extern shell_command_t __stop_shell_commands;



void cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const char *last_category = NULL;

    printk("Available commands:\n");

    for (const shell_command_t *cmd = &__start_shell_commands;
         cmd < &__stop_shell_commands;
         cmd++)
    {
        if (!last_category || strcmp(last_category, cmd->category) != 0) {
            //printk("\n[%s]\n", cmd->category);
            printk("\n=== %s ===\n", cmd->category);
            last_category = cmd->category;
        }
        printk("%s - %s\n", cmd->name, cmd->description ? cmd->description : "No description");
    }
}


SHELL_CMD("user", "help", "Show available commands", cmd_help);

#endif
