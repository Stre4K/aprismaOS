// paging.c (x86_64)
// <kernel/mm/paging.h>
//
// 4-level paging (PML4 -> PDPT -> PD -> PT). Every page table is reached
// through the direct map at PHYS_MAP_BASE, so tables are edited with P2V()
// instead of a recursive mapping. The boot tables already provide the direct
// map for the first 1 GiB, which covers every page the PMM hands out.

#include <kernel/mm/paging.h>
#include <kernel/mm/pmm.h>
#include <kernel/arch/memlayout.h>
#include <kernel/arch/cpu.h>
#include <kernel/panic.h>
#include <stdint.h>
#include <string.h>

#define PAGE_SIZE       0x1000UL
#define LARGE_PAGE_SIZE 0x200000UL   // 2 MiB
#define HUGE_PAGE_SIZE  0x40000000UL // 1 GiB

#define PTE_PRESENT   (1UL << 0)
#define PTE_RW        (1UL << 1)
#define PTE_USER      (1UL << 2)
#define PTE_HUGE      (1UL << 7)     // 2 MiB page in a PD, 1 GiB page in a PDPT
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000UL

#define PML4_INDEX(v) (((v) >> 39) & 0x1FF)
#define PDPT_INDEX(v) (((v) >> 30) & 0x1FF)
#define PD_INDEX(v)   (((v) >> 21) & 0x1FF)
#define PT_INDEX(v)   (((v) >> 12) & 0x1FF)

extern char kernel_start_phys[];
extern char kernel_end_phys[];

static inline void invlpg(uintptr_t addr)
{
    asm volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

// The PML4 currently loaded in CR3
static uint64_t *active_pml4(void)
{
    return P2V(read_cr3() & PTE_ADDR_MASK);
}

// Allocate a zeroed page table and return its physical address
static uintptr_t alloc_table(void)
{
    void *phys = pmm_alloc_page();
    if (phys == NULL)
        panic("paging: out of physical memory for page tables");

    memset(P2V(phys), 0, PAGE_SIZE);
    return (uintptr_t)phys;
}

// Follow entry `index` of `table` to the next level table, creating it if
// `create` is set. Returns NULL if it is missing and not created.
static uint64_t *next_table(uint64_t *table, unsigned index, int create)
{
    uint64_t entry = table[index];

    if (entry & PTE_PRESENT) {
        if (entry & PTE_HUGE)
            panic("paging: address lies inside a large page");
        return P2V(entry & PTE_ADDR_MASK);
    }

    if (!create)
        return NULL;

    uintptr_t phys = alloc_table();
    table[index] = phys | PTE_PRESENT | PTE_RW;
    return P2V(phys);
}

static void map_page_in(uint64_t *pml4, uintptr_t virt, uintptr_t phys)
{
    uint64_t *pdpt = next_table(pml4, PML4_INDEX(virt), 1);
    uint64_t *pd = next_table(pdpt, PDPT_INDEX(virt), 1);
    uint64_t *pt = next_table(pd, PD_INDEX(virt), 1);

    pt[PT_INDEX(virt)] = (phys & PTE_ADDR_MASK) | PTE_PRESENT | PTE_RW;
}

static void map_large_page_in(uint64_t *pml4, uintptr_t virt, uintptr_t phys)
{
    uint64_t *pdpt = next_table(pml4, PML4_INDEX(virt), 1);
    uint64_t *pd = next_table(pdpt, PDPT_INDEX(virt), 1);

    pd[PD_INDEX(virt)] = (phys & PTE_ADDR_MASK) | PTE_PRESENT | PTE_RW | PTE_HUGE;
}

void paging_init(void)
{
    uint64_t *pml4 = P2V(alloc_table());

    // Direct map of physical memory using 2 MiB pages
    for (uintptr_t phys = 0; phys < PHYS_MAP_SIZE; phys += LARGE_PAGE_SIZE)
        map_large_page_in(pml4, PHYS_MAP_BASE + phys, phys);

    // Kernel image at its link address, 4 KiB pages
    for (uintptr_t phys = (uintptr_t)kernel_start_phys;
         phys < (uintptr_t)kernel_end_phys; phys += PAGE_SIZE)
        map_page_in(pml4, KERNEL_VMA + phys, phys);

    // Switching CR3 also drops the boot identity map of low memory
    write_cr3(V2P(pml4));
}

void map_page(uintptr_t virt, uintptr_t phys)
{
    map_page_in(active_pml4(), virt, phys);
    invlpg(virt);
}

uintptr_t unmap_page(uintptr_t virt)
{
    uint64_t *pdpt = next_table(active_pml4(), PML4_INDEX(virt), 0);
    if (pdpt == NULL)
        return 0;

    uint64_t *pd = next_table(pdpt, PDPT_INDEX(virt), 0);
    if (pd == NULL)
        return 0;

    uint64_t *pt = next_table(pd, PD_INDEX(virt), 0);
    if (pt == NULL)
        return 0;

    uint64_t entry = pt[PT_INDEX(virt)];
    if (!(entry & PTE_PRESENT))
        return 0;

    pt[PT_INDEX(virt)] = 0;
    invlpg(virt);

    return entry & PTE_ADDR_MASK;
}

uintptr_t virt_to_phys(uintptr_t virt)
{
    uint64_t entry = active_pml4()[PML4_INDEX(virt)];
    if (!(entry & PTE_PRESENT))
        return 0;

    entry = ((uint64_t *)P2V(entry & PTE_ADDR_MASK))[PDPT_INDEX(virt)];
    if (!(entry & PTE_PRESENT))
        return 0;
    if (entry & PTE_HUGE)
        return (entry & PTE_ADDR_MASK & ~(HUGE_PAGE_SIZE - 1)) | (virt & (HUGE_PAGE_SIZE - 1));

    entry = ((uint64_t *)P2V(entry & PTE_ADDR_MASK))[PD_INDEX(virt)];
    if (!(entry & PTE_PRESENT))
        return 0;
    if (entry & PTE_HUGE)
        return (entry & PTE_ADDR_MASK & ~(LARGE_PAGE_SIZE - 1)) | (virt & (LARGE_PAGE_SIZE - 1));

    entry = ((uint64_t *)P2V(entry & PTE_ADDR_MASK))[PT_INDEX(virt)];
    if (!(entry & PTE_PRESENT))
        return 0;

    return (entry & PTE_ADDR_MASK) | (virt & (PAGE_SIZE - 1));
}
