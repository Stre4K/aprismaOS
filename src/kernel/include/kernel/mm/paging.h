// paging.h
// Author: Stre4K
// Date: 2026-03-08
//
// Arch-independent paging API, implemented in arch/<arch>/mm/paging.c

#ifndef __KERNEL_MM_PAGING_H__
#define __KERNEL_MM_PAGING_H__

#include <stdint.h>

// Build the kernel's page tables and switch to them
void paging_init(void);

// Map one 4 KiB page (read/write, kernel only)
void map_page(uintptr_t virt, uintptr_t phys);

// Remove a 4 KiB mapping; returns the physical page that was mapped (0 if none)
uintptr_t unmap_page(uintptr_t virt);

// Physical address behind a virtual address (0 if unmapped)
uintptr_t virt_to_phys(uintptr_t virt);

#endif // __KERNEL_MM_PAGING_H__
