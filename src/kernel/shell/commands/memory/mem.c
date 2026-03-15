// mem.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-14

#include <kernel/config.h>

#ifdef CONFIG_CMD_MEMORY

#include <kernel/shell/commands.h>

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

SHELL_CMD("memory", "mem", "Display memory information", cmd_mem);

#endif // CONFIG_CMD_MEM
