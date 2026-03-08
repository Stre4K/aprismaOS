// memory_map.c
// <kernel/mm/memory_map.h>
// Author: Stre4K
// Date: 2026-03-08

#include <kernel/multiboot.h>
#include <kernel/printk.h>

void memory_map_detect(multiboot_info_t *mbi) {
    if (!(mbi->flags & (1 << 6))) {
        printk("multiboot: no memory map\n");
        return;
    }

    printk("multiboot memory map:\n");

    uint32_t addr = mbi->mmap_addr;
    uint32_t end = addr + mbi->mmap_length;

    uint64_t usable = 0;
    uint64_t reserved = 0;

    while (addr < end) {
        multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)addr;

        printk(" base=%llx len=%llx type=%u\n", entry->addr, entry->len,
               entry->type);

        if (entry->type == 1)
            usable += entry->len;
        else
            reserved += entry->len;

        addr += entry->size + sizeof(entry->size);
    }
    printk("Usable RAM: %llu MB (%llu KB)\n", usable / 1024 / 1024, usable / 1024);
    printk("Reserved RAM: %llu MB (%llu KB)\n", reserved / 1024 / 1024, reserved / 1024);
}
