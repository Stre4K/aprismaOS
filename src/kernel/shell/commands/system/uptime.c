// uptime.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/arch/timer.h>
#include <kernel/printk.h>

void cmd_uptime(int argc, char **argv) {
    (void)argc;
    (void)argv;
    uint64_t ticks = pit_get_ticks();
    uint32_t seconds = ticks / 100;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;
    printk("Uptime: %lld days, %lld hours, %lld minutes, %lld seconds, (%lld ticks)\n", days, hours % 24, minutes % 60, seconds % 60, ticks);
}

