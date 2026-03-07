// <kernel/shell/shell.h>
// Author: Stre4K
// Date: 2026-03-06


#ifndef __KERNEL_SHELL_H__
#define __KERNEL_SHELL_H__

#include <kernel/shell/commands.h>

#include <stddef.h>

#define SHELL_MAX_COMMANDS 64
#define SHELL_MAX_ARGS 16

void shell(char *cmdline_buffer);
int shell_parse(char *line, char **argv);

int shell_register_command(const char *name, const char *description, void (*func)(int, char**));

/* getters */
size_t shell_get_num_commands(void);
const shell_command_t *shell_get_commands(void);

#endif // KERNEL_SHELL_H


