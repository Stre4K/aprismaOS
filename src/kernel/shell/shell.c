// shell.c
// <kernel/shell/shell.h>
// Author: Stre4K
// Date: 2026-03-06

#include <kernel/shell/shell.h>
#include <kernel/shell/commands.h>

#include <kernel/printk.h>
#include <string.h>

static shell_command_t commands[SHELL_MAX_COMMANDS];
static size_t num_commands = 0;

size_t shell_get_num_commands(void) {
    return num_commands;
}

const shell_command_t *shell_get_commands(void) {
    return commands;
}

int shell_register_command(const char *name, const char* description, void (*func)(int argc, char **argv)) {
    if (num_commands >= SHELL_MAX_COMMANDS) return -1;
    commands[num_commands].name = name;
    commands[num_commands].description = description;
    commands[num_commands].func = func;
    num_commands++;
    return 0;
}

void shell(char *cmdline_buffer) {
    char *argv[SHELL_MAX_ARGS];
    int argc = shell_parse(cmdline_buffer, argv);

    if (argc == 0)
        return;

    for (size_t i = 0; i < num_commands; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].func(argc, argv);
            return;
        }
    }

    printk("Unknown command: %s\n", argv[0]);
}


