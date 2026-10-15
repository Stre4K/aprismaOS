// paging.c
// <kernel/mm/paging.h>
// Author: Stre4K
// Date: 2026-03-08

#include <kernel/mm/pmm.h>
#include <kernel/panic.h>
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
    asm volatile("mov %0, %%cr3" :: "r"(pd) : "memory");

    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));

    cr0 |= 0x80000000;

    asm volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
}

static inline void invlpg(uint32_t addr)
{
    asm volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

static int paging_enabled(void)
{
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    return (cr0 & 0x80000000) != 0;
}

// Map one 4K page. Only valid once paging is on, since it edits the
// page tables through the recursive mapping at 0xFFC00000.
void map_page(uint32_t virt, uint32_t phys)
{
    uint32_t dir = virt >> 22;
    uint32_t table = (virt >> 12) & 0x3FF;

    if (!paging_enabled())
        panic("map_page: paging is not enabled");

    // Slot 1023 is the recursive mapping; writing there would corrupt the page directory
    if (dir == 1023)
        panic("map_page: cannot map into the recursive mapping region");

    uint32_t *pd = PAGE_DIRECTORY;
    uint32_t *pt = PAGE_TABLE(virt);

    if (!(pd[dir] & PAGE_PRESENT)) {
        uint32_t new_table = (uint32_t)(uintptr_t)pmm_alloc_page();
        if (new_table == 0)
            panic("map_page: out of physical memory for page table");

        pd[dir] = new_table | PAGE_PRESENT | PAGE_RW;

        // The table's physical page may not be mapped anywhere, so clear it
        // through its recursive mapping instead of its physical address
        invlpg((uint32_t)(uintptr_t)pt);
        memset(pt, 0, 4096);
    }

    pt[table] = (phys & ~0xFFF) | PAGE_PRESENT | PAGE_RW;
    invlpg(virt);
}

// Remove the mapping for one 4K page. Returns the physical address that was
// mapped there (0 if nothing was), so the caller can give it back to the PMM.
uint32_t unmap_page(uint32_t virt)
{
    uint32_t dir = virt >> 22;
    uint32_t table = (virt >> 12) & 0x3FF;

    if (dir == 1023)
        panic("unmap_page: cannot unmap the recursive mapping region");

    uint32_t *pd = PAGE_DIRECTORY;
    if (!(pd[dir] & PAGE_PRESENT))
        return 0;

    uint32_t *pt = PAGE_TABLE(virt);
    if (!(pt[table] & PAGE_PRESENT))
        return 0;

    uint32_t phys = pt[table] & ~0xFFF;
    pt[table] = 0;
    invlpg(virt);

    return phys;
}

// Look up the physical address a virtual address maps to (0 if unmapped)
uint32_t virt_to_phys(uint32_t virt)
{
    uint32_t dir = virt >> 22;
    uint32_t table = (virt >> 12) & 0x3FF;

    uint32_t *pd = PAGE_DIRECTORY;
    if (!(pd[dir] & PAGE_PRESENT))
        return 0;

    uint32_t *pt = PAGE_TABLE(virt);
    if (!(pt[table] & PAGE_PRESENT))
        return 0;

    return (pt[table] & ~0xFFF) | (virt & 0xFFF);
}
