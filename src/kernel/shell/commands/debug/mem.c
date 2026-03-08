// mem.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/multiboot.h>
#include <kernel/printk.h>
#include <stdint.h>


void cmd_mem(int argc, char **argv) {
    (void)argc;
    (void)argv;

    multiboot_info_t *multiboot_info = multiboot_get_info();
    uint32_t mem_lower = multiboot_info->mem_lower;
    uint32_t mem_upper = multiboot_info->mem_upper;

    printk("Lower memory: %u KB\n", mem_lower);
    printk("Upper memory: %u KB\n", mem_upper);
}
