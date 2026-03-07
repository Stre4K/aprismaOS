// commands.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/shell/commands.h>
#include <kernel/shell/shell.h>

void init_shell_commands(void) {
    shell_register_command("clear", "Clear the screen", cmd_clear);
    shell_register_command("echo", "Print text", cmd_echo);
    shell_register_command("help", "Show available commands", cmd_help);
}
