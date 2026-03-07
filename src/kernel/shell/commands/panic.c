// panic.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/panic.h>

void cmd_panic(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    panic("Manual panic from shell");
}
