// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-06

#ifndef __KERNEL_SHELL_COMMANDS_H__
#define __KERNEL_SHELL_COMMANDS_H__


typedef struct {
    const char *name;
    const char *description;
    void (*func)(int argc, char **argv);
} shell_command_t;


void cmd_clear(int argc, char **argv);
void cmd_echo(int argc, char **argv);
void cmd_help(int argc, char **argv);

void init_shell_commands();

#endif // KERNEL_SHELL_COMMANDS_H
