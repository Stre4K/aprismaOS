// clear.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/drivers/tty.h>

void cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    terminal_clear_screen();
}
