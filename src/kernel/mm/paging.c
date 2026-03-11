// paging.c
// <kernel/mm/paging.h>
// Author: Stre4K
// Date: 2026-03-08

#include <kernel/mm/pmm.h>
#include <stdint.h>
#include <string.h>

#define PAGE_DIRECTORY ((uint32_t*)0xFFFFF000)
#define PAGE_TABLES    ((uint32_t*)0xFFC00000)

#define PAGE_TABLE(addr) ((uint32_t*) (0xFFC00000 + (((addr) >> 22) * 0x1000)))

#define PAGE_PRESENT  (1 << 0)   // Page is present in memory. If 0, access causes a page fault.
#define PAGE_RW       (1 << 1)   // Read/write permission. 0 = read-only, 1 = read/write.
#define PAGE_USER     (1 << 2)   // User-mode access allowed. 0 = supervisor/kernel only.
#define PAGE_PWT      (1 << 3)   // Page-level write-through. 0 = write-back caching.
#define PAGE_PCD      (1 << 4)   // Page-level cache disable. 0 = caching enabled.
#define PAGE_ACCESSED (1 << 5)   // Set by CPU when page is read or written.
#define PAGE_DIRTY    (1 << 6)   // Set by CPU when page is written (only for PTEs).
#define PAGE_PAT      (1 << 7)   // Page Attribute Table index (4KB page) or Page Size (large pages in PDE).
#define PAGE_GLOBAL   (1 << 8)   // Global page. TLB entries not flushed on context switch if CR4.PGE = 1.


uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t first_page_table[1024] __attribute__((aligned(4096)));

void paging_init()
{
    for (int i = 0; i < 1024; i++)
        first_page_table[i] = (i * 0x1000) | PAGE_PRESENT | PAGE_RW;

    page_directory[0] = ((uint32_t)first_page_table) | PAGE_PRESENT | PAGE_RW;

    for (int i = 1; i < 1024; i++)
        page_directory[i] = 0;

    // Recursive mapping
    page_directory[1023] = ((uint32_t)page_directory) | PAGE_PRESENT | PAGE_RW;
}


void enable_paging(uint32_t *pd)
{
    asm volatile("mov %0, %%cr3" :: "r"(pd));

    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));

    cr0 |= 0x80000000;

    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

void map_page(uint32_t virt, uint32_t phys)
{
    uint32_t dir = virt >> 22;
    uint32_t table = (virt >> 12) & 0x3FF;

    uint32_t *pd = PAGE_DIRECTORY;

    if (!(pd[dir] & 1)) {
        uint32_t *new_table = pmm_alloc_page();
        memset(new_table, 0, 4096);

        pd[dir] = ((uint32_t)new_table) | 3;
    }

    uint32_t *pt = PAGE_TABLE(virt);
    pt[table] = phys | 3;
}
