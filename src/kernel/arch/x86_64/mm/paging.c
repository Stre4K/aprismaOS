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

#ifdef CONFIG_USERMODE
static uintptr_t kernel_space;  // physical address of the kernel's PML4
#endif

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

#ifdef CONFIG_USERMODE
    // Create every kernel-half PML4 entry now. Each process's top-level
    // table copies these entries, so they must never change afterwards:
    // new kernel mappings then appear in every address space.
    for (unsigned i = 256; i < 512; i++) {
        if (!(pml4[i] & PTE_PRESENT))
            pml4[i] = alloc_table() | PTE_PRESENT | PTE_RW;
    }
    kernel_space = V2P(pml4);
#endif

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

// Physical memory below PHYS_MAP_SIZE is always reachable through the
// direct map
void *paging_map_physical(uintptr_t phys, size_t size)
{
    if (phys + size < phys || phys + size > PHYS_MAP_SIZE)
        return NULL;
    return P2V(phys);
}

#ifdef CONFIG_USERMODE
/* ---- per-process address spaces ---- */

#define PML4_USER_ENTRIES 256  // entries 0..255 map the lower (user) half

uintptr_t paging_kernel_space(void)
{
    return kernel_space;
}

// Allocate a zeroed page table; 0 when out of memory (user paths must not panic)
static uintptr_t try_alloc_table(void)
{
    void *phys = pmm_alloc_page();
    if (phys != NULL)
        memset(P2V(phys), 0, PAGE_SIZE);
    return (uintptr_t)phys;
}

uintptr_t paging_create_space(void)
{
    uintptr_t space = try_alloc_table();
    if (space == 0)
        return 0;

    uint64_t *pml4 = P2V(space);
    uint64_t *kernel_pml4 = P2V(kernel_space);
    for (unsigned i = PML4_USER_ENTRIES; i < 512; i++)
        pml4[i] = kernel_pml4[i];

    return space;
}

// Free a page table level and everything below it. Levels: 3 = PDPT,
// 2 = PD, 1 = PT (whose entries are the mapped user pages).
static void free_user_table(uint64_t *table, int level)
{
    for (unsigned i = 0; i < 512; i++) {
        uint64_t entry = table[i];
        if (!(entry & PTE_PRESENT))
            continue;
        if (level > 1)
            free_user_table(P2V(entry & PTE_ADDR_MASK), level - 1);
        else
            pmm_free_page((void *)(uintptr_t)(entry & PTE_ADDR_MASK));
    }
    pmm_free_page((void *)V2P(table));
}

void paging_destroy_space(uintptr_t space)
{
    if (space == (read_cr3() & PTE_ADDR_MASK))
        panic("paging_destroy_space: space is active");

    uint64_t *pml4 = P2V(space);
    for (unsigned i = 0; i < PML4_USER_ENTRIES; i++) {
        if (pml4[i] & PTE_PRESENT)
            free_user_table(P2V(pml4[i] & PTE_ADDR_MASK), 3);
    }
    pmm_free_page((void *)space);  // kernel-half entries are shared, not freed
}

void paging_switch_space(uintptr_t space)
{
    if ((read_cr3() & PTE_ADDR_MASK) != space)
        write_cr3(space);
}

// Next level of a user mapping. Every level needs PTE_USER, or the CPU
// denies user access regardless of the final entry.
static uint64_t *next_user_table(uint64_t *table, unsigned index)
{
    uint64_t entry = table[index];
    if (entry & PTE_PRESENT)
        return P2V(entry & PTE_ADDR_MASK);

    uintptr_t phys = try_alloc_table();
    if (phys == 0)
        return NULL;
    table[index] = phys | PTE_PRESENT | PTE_RW | PTE_USER;
    return P2V(phys);
}

bool paging_map_user(uintptr_t space, uintptr_t virt, uintptr_t phys, bool writable)
{
    if (virt < USER_START || virt >= USER_END || (virt & (PAGE_SIZE - 1)))
        return false;

    uint64_t *pdpt = next_user_table(P2V(space), PML4_INDEX(virt));
    uint64_t *pd = pdpt ? next_user_table(pdpt, PDPT_INDEX(virt)) : NULL;
    uint64_t *pt = pd ? next_user_table(pd, PD_INDEX(virt)) : NULL;
    if (pt == NULL || (pt[PT_INDEX(virt)] & PTE_PRESENT))
        return false;

    pt[PT_INDEX(virt)] = (phys & PTE_ADDR_MASK) | PTE_PRESENT | PTE_USER | (writable ? PTE_RW : 0);
    return true;
}

bool paging_user_access_ok(uintptr_t virt, bool write)
{
    if (virt < USER_START || virt >= USER_END)
        return false;

    uint64_t required = PTE_PRESENT | PTE_USER | (write ? PTE_RW : 0);
    uint64_t *table = active_pml4();
    unsigned indices[4] = { PML4_INDEX(virt), PDPT_INDEX(virt), PD_INDEX(virt), PT_INDEX(virt) };

    for (int level = 0; level < 4; level++) {
        uint64_t entry = table[indices[level]];
        if ((entry & required) != required || (entry & PTE_HUGE))
            return false;
        if (level < 3)
            table = P2V(entry & PTE_ADDR_MASK);
    }
    return true;
}
#endif // CONFIG_USERMODE
