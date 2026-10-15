// memlayout.h (x86_64)
// Virtual memory layout and physical <-> virtual helpers
//
//   0x0000000000000000 - 0x00007FFFFFFFFFFF  user space (unused for now)
//   0xFFFF800000000000 - ...                 direct map of physical memory
//   0xFFFFC00000000000 - +256 MiB            kernel heap
//   0xFFFFFFFF80000000 - ...                 kernel image (-mcmodel=kernel)

#ifndef _KERNEL_ARCH_MEMLAYOUT_H
#define _KERNEL_ARCH_MEMLAYOUT_H

#include <stdint.h>

#define PHYS_MAP_BASE 0xFFFF800000000000UL
#define PHYS_MAP_SIZE 0x100000000UL         // 4 GiB, covers what the PMM manages

#define KERNEL_VMA    0xFFFFFFFF80000000UL

// Kernel heap
#define HEAP_START 0xFFFFC00000000000UL
#define HEAP_MAX   0xFFFFC00010000000UL     // 256 MiB

// Direct-map conversions
#define P2V(phys) ((void *)((uintptr_t)(phys) + PHYS_MAP_BASE))
#define V2P(virt) ((uintptr_t)(virt) - PHYS_MAP_BASE)

#endif
