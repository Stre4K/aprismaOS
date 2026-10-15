// clear.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-14

#include <kernel/config.h>

#ifdef CONFIG_CMD_USER

#include <kernel/shell/commands.h>

#include <kernel/drivers/tty.h>

void cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    terminal_clear_screen();
}

SHELL_CMD("user", "clear", "Clear the screen", cmd_clear);

#endif // CONFIG_CMD_USER
