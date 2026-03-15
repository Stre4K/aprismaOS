// shell.c
// <kernel/shell/shell.h>
// Author: Stre4K
// Date: 2026-03-06

#include <kernel/shell/shell.h>
#include <kernel/shell/commands.h>

#include <kernel/printk.h>
#include <string.h>

extern shell_command_t __start_shell_commands;
extern shell_command_t __stop_shell_commands;

void shell(char *cmdline_buffer)
{
    char *argv[SHELL_MAX_ARGS];
    int argc = shell_parse(cmdline_buffer, argv);

    if (argc == 0)
        return;

    for (const shell_command_t *cmd = &__start_shell_commands;
         cmd < &__stop_shell_commands;
         cmd++)
    {
        if (strcmp(argv[0], cmd->name) == 0) {
            cmd->func(argc, argv);
            return;
        }
    }

    printk("Unknown command: %s\n", argv[0]);
}
