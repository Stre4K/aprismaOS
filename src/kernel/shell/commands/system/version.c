// version.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-14

#include <kernel/config.h>

#ifdef CONFIG_CMD_SYSTEM

#include <kernel/shell/commands.h>

#include <kernel/version.h>
#include <kernel/printk.h>

void cmd_version(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printk("%s v%s (%s)\n", OS_NAME, OS_VERSION, OS_ARCH);
    printk("Built: %s %s\n", __DATE__, __TIME__);
}

SHELL_CMD("system", "version", "Display version information", cmd_version);

#endif // CONFIG_CMD_SYSTEM
