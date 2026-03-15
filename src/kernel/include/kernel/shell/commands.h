// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-15

#ifndef __KERNEL_SHELL_COMMANDS_H__
#define __KERNEL_SHELL_COMMANDS_H__

typedef struct {
    const char *category;
    const char *name;
    const char *description;
    void (*func)(int argc, char **argv);
} shell_command_t;

#define SHELL_CMD(cat, name, desc, fn) \
static const shell_command_t shell_cmd_##fn \
__attribute__((section(".shell_commands"), used)) = { \
    cat, name, desc, fn \
}

#endif // KERNEL_SHELL_COMMANDS_H
