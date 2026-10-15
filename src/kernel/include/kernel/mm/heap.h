// heap.h
// Author: Stre4k
// Date: 2026-10-15

#ifndef __KERNEL_MM_HEAP_H__
#define __KERNEL_MM_HEAP_H__

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t heap_size;     // bytes currently mapped for the heap
    size_t used_bytes;    // payload bytes in allocated blocks
    size_t free_bytes;    // payload bytes in free blocks
    size_t used_blocks;
    size_t free_blocks;
} kheap_stats_t;

// Must be called after paging is enabled and the PMM is initialised
void kheap_init(void);

void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);

void kheap_get_stats(kheap_stats_t *stats);
void kheap_dump(void);

// Walk every block and verify the heap's invariants; returns 1 if consistent
int kheap_check(void);

#endif // __KERNEL_MM_HEAP_H__
