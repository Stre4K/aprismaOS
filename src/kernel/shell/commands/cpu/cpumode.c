// cpumode.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-14

#include <kernel/config.h>

#ifdef CONFIG_CMD_CPU

#include <kernel/shell/commands.h>

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

SHELL_CMD("cpu", "cpumode", "Display CPU mode", cmd_cpumode);

#endif // CONFIG_CMD_CPU
