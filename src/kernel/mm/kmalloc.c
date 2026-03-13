// kmalloc.c
// <kernel/mm/kmalloc.h>
// Author: Stre4k
// Date: 2026-03-08

#include <stdint.h>
#include <stddef.h>
#include <kernel/mm/pmm.h>
#include <kernel/mm/paging.h>

/*
static uint8_t *heap_start = (uint8_t*)0x1000000; // pick a free virtual address
static uint8_t *heap_end;

void heap_init() {
    heap_end = heap_start;
}

void* kmalloc(uint32_t size)
{
    // align size to 4 bytes
    size = (size + 3) & ~3;

    // allocate page if heap_end crosses page boundary
    if ((uintptr_t)(heap_end + size) & 0xFFF) {
        void *page = pmm_alloc_page();
        if (!page) return NULL;
        map_page((uintptr_t)page, (uintptr_t)page);
    }

    void *ptr = heap_end;
    heap_end += size;
    return ptr;
}
*/

static uint8_t *heap_start = (uint8_t*)0x1000000;
static uint8_t *heap_end;
static uintptr_t heap_page_end;

void heap_init()
{
    heap_end = heap_start;
    heap_page_end = (uintptr_t)heap_start;
}

void* kmalloc(uint32_t size)
{
    size = (size + 3) & ~3;

    if ((uintptr_t)heap_end + size >= heap_page_end)
    {
        void *phys = pmm_alloc_page();
        if (!phys) return NULL;

        map_page(heap_page_end, (uintptr_t)phys);

        heap_page_end += 0x1000;
    }

    void *ptr = heap_end;
    heap_end += size;
    return ptr;
}
