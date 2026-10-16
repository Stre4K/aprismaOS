// paging.h
// Author: Stre4K
// Date: 2026-03-08
//
// Arch-independent paging API, implemented in arch/<arch>/mm/paging.c

#ifndef __KERNEL_MM_PAGING_H__
#define __KERNEL_MM_PAGING_H__

#include <stddef.h>
#include <stdint.h>

// Build the kernel's page tables and switch to them
void paging_init(void);

// Map one 4 KiB page (read/write, kernel only)
void map_page(uintptr_t virt, uintptr_t phys);

// Remove a 4 KiB mapping; returns the physical page that was mapped (0 if none)
uintptr_t unmap_page(uintptr_t virt);

// Physical address behind a virtual address (0 if unmapped)
uintptr_t virt_to_phys(uintptr_t virt);

// Make a range of physical memory readable by the kernel (e.g. a boot
// module) and return its virtual address; NULL if that is not possible
void *paging_map_physical(uintptr_t phys, size_t size);

#ifdef CONFIG_USERMODE
#include <stdbool.h>

// Per-process address spaces. A space is the physical address of its
// top-level page table; the kernel half is shared by every space.

// The kernel's own space (used by kernel threads)
uintptr_t paging_kernel_space(void);

// New space with the kernel half and an empty user half; 0 when out of memory
uintptr_t paging_create_space(void);

// Free every user page and page table of a space, then the space itself.
// Must not be the active space.
void paging_destroy_space(uintptr_t space);

// Load a space into the MMU (no-op if it is already active)
void paging_switch_space(uintptr_t space);

// Map one user page into a space; false if the address is not user space,
// is already mapped or page tables cannot be allocated
bool paging_map_user(uintptr_t space, uintptr_t virt, uintptr_t phys, bool writable);

// Remove one user page from a space (flushing the TLB if it is active);
// returns the physical page that was mapped, 0 if none
uintptr_t paging_unmap_user(uintptr_t space, uintptr_t virt);

// True if the current space lets user mode read (or write) this address
bool paging_user_access_ok(uintptr_t virt, bool write);
#endif

#endif // __KERNEL_MM_PAGING_H__
