// version.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/version.h>
#include <kernel/printk.h>

void cmd_version(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printk("%s v%s (%s)\n", OS_NAME, OS_VERSION, OS_ARCH);
    printk("Built: %s %s\n", __DATE__, __TIME__);
}
