// reboot.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

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
