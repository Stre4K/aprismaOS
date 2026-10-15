// heap.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-10-15

#include <kernel/config.h>

#ifdef CONFIG_CMD_MEMORY

#include <kernel/shell/commands.h>

#include <kernel/mm/heap.h>
#include <kernel/mm/pmm.h>
#include <kernel/printk.h>
#include <string.h>

void cmd_heap(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "blocks") == 0) {
        kheap_dump();
        return;
    }

    if (argc > 1 && strcmp(argv[1], "check") == 0) {
        printk("heap is %s\n", kheap_check() ? "consistent" : "CORRUPTED");
        return;
    }

    kheap_stats_t s;
    kheap_get_stats(&s);

    printk("Heap size: %u KB\n", (uint32_t)(s.heap_size / 1024));
    printk("Used:      %u bytes in %u blocks\n", (uint32_t)s.used_bytes, (uint32_t)s.used_blocks);
    printk("Free:      %u bytes in %u blocks\n", (uint32_t)s.free_bytes, (uint32_t)s.free_blocks);
    printk("PMM:       %u of %u pages free\n", pmm_free_pages(), pmm_total_pages());
    printk("Usage: heap [blocks|check]\n");
}

SHELL_CMD("memory", "heap", "Display kernel heap usage", cmd_heap);

#endif // CONFIG_CMD_MEMORY
