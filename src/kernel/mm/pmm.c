// pmm.c
// <kernel/mm/pmm.h>
// Author: Stre4k
// Date: 2026-03-08

#include <kernel/bitmap.h>
#include <kernel/multiboot.h>
#include <stdint.h>
#include <string.h>

#define PAGE_SIZE 4096
#define MAX_MEMORY (1024 * 1024 * 1024)  // 1GB

static uint8_t bitmap[MAX_MEMORY / PAGE_SIZE / 8];

extern uint32_t kernel_end;

void pmm_init(multiboot_info_t *mbi) {

    memset(bitmap, 0xFF, sizeof(bitmap));

    uint32_t addr = mbi->mmap_addr;
    uint32_t end = addr + mbi->mmap_length;

    while (addr < end) {

        multiboot_mmap_entry_t *entry =
            (multiboot_mmap_entry_t *)(uintptr_t)addr;

        if (entry->type == 1) {

            uint64_t start = entry->addr;
            uint64_t length = entry->len;

            for (uint64_t i = start; i < start + length; i += PAGE_SIZE) {
                /*protect first 1MB */
                if (i < 0x100000)
                    continue;

                /* protect kernel */
                if (i < (uintptr_t)&kernel_end)
                    continue;

                /* protect multiboot info */
                uint64_t mmap_start = mbi->mmap_addr;
                uint64_t mmap_end = mbi->mmap_addr + mbi->mmap_length;

                if (i >= mmap_start && i < mmap_end)
                    continue;

                uint32_t page = i / PAGE_SIZE;

                if (page < sizeof(bitmap) * 8)
                    clear_bit(bitmap, page);
            }
        }

        addr += entry->size + sizeof(entry->size);
    }
}

void *pmm_alloc_page() {
    for (uint32_t i = 0; i < sizeof(bitmap) * 8; i++) {
        if (!test_bit(bitmap, i)) {
            set_bit(bitmap, i);
            return (void *)(uintptr_t)(i * PAGE_SIZE);
        }
    }
    return NULL;
}

void pmm_free_page(void *page) {
    uintptr_t addr = (uintptr_t)page;

    /*protect kernel*/
    if (addr < (uintptr_t)&kernel_end)
        return;

    uint32_t index = addr / PAGE_SIZE;
    clear_bit(bitmap, index);
}

uint32_t pmm_total_pages() {
    return sizeof(bitmap) * 8;
}

uint32_t pmm_free_pages() {
    uint32_t count = 0;
    for (uint32_t i = 0; i < sizeof(bitmap) * 8; i++) {
        if (!test_bit(bitmap, i))
            count++;
    }
    return count;
}
