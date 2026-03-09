// mem.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/multiboot.h>
#include <kernel/printk.h>
#include <kernel/mm/memory_map.h>

void cmd_mem(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    multiboot_info_t *mbi = multiboot_get_info();

    printk("Lower memory: %u MB (%u KB)\n", mbi->mem_lower/1024, mbi->mem_lower);
    printk("Upper memory: %u MB (%u KB)\n", mbi->mem_upper/1024, mbi->mem_upper);

    memory_map_detect(mbi);
}
