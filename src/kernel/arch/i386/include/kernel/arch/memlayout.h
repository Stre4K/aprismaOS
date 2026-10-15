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

// Direct-map conversions; only valid for memory inside the identity map
#define P2V(phys) ((void *)((uintptr_t)(phys) + PHYS_MAP_BASE))
#define V2P(virt) ((uintptr_t)(virt) - PHYS_MAP_BASE)

#endif
