// memlayout.h (x86_64)
// Virtual memory layout and physical <-> virtual helpers
//
//   0x0000000000400000 - 0x00007FFFFFFFFFFF  user space (per process)
//   0xFFFF800000000000 - ...                 direct map of physical memory
//   0xFFFFC00000000000 - +256 MiB            kernel heap
//   0xFFFFD00000000000 - +16 MiB             kernel thread stacks
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

// Kernel thread stacks: one slot per thread. Only the top KSTACK_PAGES of
// a slot are mapped; the unmapped pages below catch stack overflows.
#define KSTACK_START     0xFFFFD00000000000UL
#define KSTACK_SLOT_SIZE 0x10000UL            // 64 KiB per slot
#define KSTACK_PAGES     4                    // 16 KiB stack
#define KSTACK_SLOTS     256                  // maximum number of threads

// User space (lower half, one address space per process). Programs are
// linked at USER_START; the stack sits just below USER_STACK_TOP.
#define USER_START       0x0000000000400000UL
#define USER_END         0x0000800000000000UL  // end of the canonical lower half
#define USER_STACK_TOP   0x00007FFFFFFFF000UL
#define USER_STACK_PAGES 4                     // 16 KiB
#define USER_HEAP_MAX    0x4000000UL           // 64 MiB of heap (brk) per process, after the program

// Direct-map conversions
#define P2V(phys) ((void *)((uintptr_t)(phys) + PHYS_MAP_BASE))
#define V2P(virt) ((uintptr_t)(virt) - PHYS_MAP_BASE)

#endif
