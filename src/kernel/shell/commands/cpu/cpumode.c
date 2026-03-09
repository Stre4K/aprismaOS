// cpumode.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07


#include <kernel/arch/cpu.h>
#include <kernel/printk.h>

void cmd_cpumode(int argc, char **argv) {
    (void)argc;
    (void)argv;

    cpu_mode_t mode = cpu_get_mode();
    printk("CPU Mode: %s\n", cpu_mode_str(mode));

    if (cpu_is_paging_enabled())
        printk("Paging: Enabled\n");
    else
        printk("Paging: Disabled\n");

    printk("CPL: Ring %d\n", cpu_get_cpl());
}
