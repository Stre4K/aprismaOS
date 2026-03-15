// panic.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-14


#include <kernel/config.h>

#ifdef CONFIG_CMD_DEBUG

#include <kernel/shell/commands.h>


#include <kernel/panic.h>

void cmd_panic(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    panic("Manual panic from shell");
}

SHELL_CMD("debug", "panic", "Panic the system", cmd_panic);

#endif // CONFIG_CMD_DEBUG
