// memlayout.h (i386)
// Virtual memory layout and physical <-> virtual helpers

#ifndef _KERNEL_ARCH_MEMLAYOUT_H
#define _KERNEL_ARCH_MEMLAYOUT_H

#include <stdint.h>

// The kernel runs identity mapped (first 4 MiB), so physical memory used
// during boot is reachable at its own address
#define PHYS_MAP_BASE 0x00000000UL

// Kernel heap
#define HEAP_START 0xD0000000UL
#define HEAP_MAX   0xE0000000UL  // 256 MiB

// Kernel thread stacks: one slot per thread. Only the top KSTACK_PAGES of
// a slot are mapped; the unmapped pages below catch stack overflows.
#define KSTACK_START     0xE0000000UL
#define KSTACK_SLOT_SIZE 0x10000UL    // 64 KiB per slot
#define KSTACK_PAGES     4            // 16 KiB stack
#define KSTACK_SLOTS     256          // maximum number of threads (16 MiB)

// Direct-map conversions; only valid for memory inside the identity map
#define P2V(phys) ((void *)((uintptr_t)(phys) + PHYS_MAP_BASE))
#define V2P(virt) ((uintptr_t)(virt) - PHYS_MAP_BASE)

#endif
