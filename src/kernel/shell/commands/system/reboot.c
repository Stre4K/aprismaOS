// reboot.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-14


#include <kernel/config.h>

#ifdef CONFIG_CMD_SYSTEM

#include <kernel/shell/commands.h>
#include <kernel/arch/portio.h>
#include <kernel/printk.h>


void cmd_reboot(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printk("Rebooting...\n");

    // wait until keyboard controller ready
    while (inb(0x64) & 0x02);

    outb(0x64, 0xFE);

    // if reboot fails
    for (;;)
        __asm__ volatile("hlt");
}


SHELL_CMD("system", "reboot", "Reboot the system", cmd_reboot);

#endif // CONFIG_CMD_SYSTEM
