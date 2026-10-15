// heap.c
// <kernel/mm/heap.h>
// Author: Stre4k
// Date: 2026-10-15
//
// Kernel heap: a first-fit allocator over one contiguous virtual region.
// Every block (used or free) starts with a header, and all blocks are kept in
// one address-ordered list. The heap grows by mapping fresh PMM pages at its
// end and gives whole pages back when a large free block sits at the end.

#include <kernel/mm/heap.h>
#include <kernel/mm/paging.h>
#include <kernel/mm/pmm.h>
#include <kernel/arch/memlayout.h>
#include <kernel/panic.h>
#include <kernel/printk.h>
#include <stdint.h>
#include <string.h>

#define PAGE_SIZE 4096

// HEAP_START and HEAP_MAX come from <kernel/arch/memlayout.h>
#define HEAP_INITIAL_PAGES 16          // 64 KiB mapped at init
#define HEAP_TRIM_PAGES    16          // only give memory back in chunks this big

#define HEAP_ALIGN 16

#define ALIGN_UP(x, a)   (((x) + ((a) - 1)) & ~((uintptr_t)(a) - 1))

#define BLOCK_USED 0xA110CA7E
#define BLOCK_FREE 0xF4EEB10C

typedef struct block {
    uint32_t magic;         // BLOCK_USED or BLOCK_FREE, anything else is corruption
    size_t size;            // payload size in bytes, excluding the header
    struct block *prev;     // previous block by address
    struct block *next;     // next block by address
} block_t;

#define HEADER_SIZE ALIGN_UP(sizeof(block_t), HEAP_ALIGN)
#define MIN_SPLIT   (HEADER_SIZE + HEAP_ALIGN)

static block_t *heap_first;
static block_t *heap_last;
static uintptr_t heap_end;  // first unmapped address after the heap

/* ---- interrupt-safe locking (single CPU) ---- */

static inline uintptr_t heap_lock(void)
{
    uintptr_t flags;
    asm volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static inline void heap_unlock(uintptr_t flags)
{
    asm volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
}

/* ---- helpers ---- */

static inline void *block_data(block_t *b)
{
    return (uint8_t *)b + HEADER_SIZE;
}

// Map `pages` new pages at heap_end and add them to the heap.
// Returns 0 if the virtual region or physical memory is exhausted.
static int heap_grow(size_t pages)
{
    uintptr_t old_end = heap_end;

    if (pages > (HEAP_MAX - heap_end) / PAGE_SIZE)
        return 0;

    for (size_t i = 0; i < pages; i++) {
        void *phys = pmm_alloc_page();

        if (phys == NULL) {
            // Roll back what this call mapped so far
            for (uintptr_t va = old_end; va < heap_end; va += PAGE_SIZE)
                pmm_free_page((void *)(uintptr_t)unmap_page(va));
            heap_end = old_end;
            return 0;
        }

        map_page(heap_end, (uintptr_t)phys);
        heap_end += PAGE_SIZE;
    }

    size_t added = heap_end - old_end;

    if (heap_last && heap_last->magic == BLOCK_FREE) {
        heap_last->size += added;
    } else {
        block_t *b = (block_t *)old_end;
        b->magic = BLOCK_FREE;
        b->size = added - HEADER_SIZE;
        b->prev = heap_last;
        b->next = NULL;

        if (heap_last)
            heap_last->next = b;
        else
            heap_first = b;
        heap_last = b;
    }

    return 1;
}

// Shrink b to `size` bytes and turn the rest into a new free block
static void block_split(block_t *b, size_t size)
{
    if (b->size < size + MIN_SPLIT)
        return;

    block_t *rest = (block_t *)((uint8_t *)block_data(b) + size);
    rest->magic = BLOCK_FREE;
    rest->size = b->size - size - HEADER_SIZE;
    rest->prev = b;
    rest->next = b->next;

    if (b->next)
        b->next->prev = rest;
    else
        heap_last = rest;

    b->next = rest;
    b->size = size;
}

// Merge b with the block after it (both must be free)
static void block_merge_next(block_t *b)
{
    block_t *n = b->next;

    b->size += HEADER_SIZE + n->size;
    b->next = n->next;

    if (n->next)
        n->next->prev = b;
    else
        heap_last = b;

    n->magic = 0;
}

// Give whole pages at the end of the heap back to the PMM
static void heap_trim(void)
{
    block_t *b = heap_last;

    if (b->magic != BLOCK_FREE)
        return;

    uintptr_t keep = ALIGN_UP((uintptr_t)block_data(b) + HEAP_ALIGN, PAGE_SIZE);
    uintptr_t min_end = HEAP_START + HEAP_INITIAL_PAGES * PAGE_SIZE;

    if (keep < min_end)
        keep = min_end;

    if (keep >= heap_end || heap_end - keep < HEAP_TRIM_PAGES * PAGE_SIZE)
        return;

    for (uintptr_t va = keep; va < heap_end; va += PAGE_SIZE)
        pmm_free_page((void *)(uintptr_t)unmap_page(va));

    heap_end = keep;
    b->size = heap_end - (uintptr_t)block_data(b);
}

// Turn a pointer from kmalloc back into its block, or panic if it isn't one
static block_t *ptr_to_block(void *ptr, const char *caller)
{
    uintptr_t addr = (uintptr_t)ptr;

    if (addr < HEAP_START + HEADER_SIZE || addr >= heap_end || (addr & (HEAP_ALIGN - 1))) {
        printk("%s: %p is not a heap pointer\n", caller, ptr);
        panic("heap: invalid pointer");
    }

    block_t *b = (block_t *)(addr - HEADER_SIZE);

    if (b->magic == BLOCK_FREE) {
        printk("%s: %p was already freed\n", caller, ptr);
        panic("heap: double free");
    }

    if (b->magic != BLOCK_USED) {
        printk("%s: bad block header at %p\n", caller, (void *)b);
        panic("heap: corrupted block header");
    }

    return b;
}

/* ---- public API ---- */

void kheap_init(void)
{
    heap_first = NULL;
    heap_last = NULL;
    heap_end = HEAP_START;

    if (!heap_grow(HEAP_INITIAL_PAGES))
        panic("kheap_init: not enough memory for the initial heap");
}

void *kmalloc(size_t size)
{
    if (heap_first == NULL)
        panic("kmalloc: heap is not initialised");

    if (size == 0 || size > HEAP_MAX - HEAP_START)
        return NULL;

    size = ALIGN_UP(size, HEAP_ALIGN);

    uintptr_t flags = heap_lock();

    block_t *b = heap_first;
    while (b && !(b->magic == BLOCK_FREE && b->size >= size))
        b = b->next;

    if (b == NULL) {
        // No free block is big enough: extend the heap so the last block fits
        size_t needed = size + HEADER_SIZE;
        if (heap_last->magic == BLOCK_FREE)
            needed = size - heap_last->size;

        if (!heap_grow(ALIGN_UP(needed, PAGE_SIZE) / PAGE_SIZE)) {
            heap_unlock(flags);
            return NULL;
        }
        b = heap_last;
    }

    block_split(b, size);
    b->magic = BLOCK_USED;

    heap_unlock(flags);
    return block_data(b);
}

void *kcalloc(size_t count, size_t size)
{
    if (size != 0 && count > (size_t)-1 / size)
        return NULL;

    void *ptr = kmalloc(count * size);
    if (ptr)
        memset(ptr, 0, count * size);
    return ptr;
}

void kfree(void *ptr)
{
    if (ptr == NULL)
        return;

    uintptr_t flags = heap_lock();

    block_t *b = ptr_to_block(ptr, "kfree");
    b->magic = BLOCK_FREE;

    if (b->next && b->next->magic == BLOCK_FREE)
        block_merge_next(b);

    if (b->prev && b->prev->magic == BLOCK_FREE) {
        b = b->prev;
        block_merge_next(b);
    }

    if (b == heap_last)
        heap_trim();

    heap_unlock(flags);
}

void *krealloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return kmalloc(size);

    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    if (size > HEAP_MAX - HEAP_START)
        return NULL;

    uintptr_t flags = heap_lock();

    block_t *b = ptr_to_block(ptr, "krealloc");
    size_t aligned = ALIGN_UP(size, HEAP_ALIGN);

    // Grow in place by absorbing a free neighbour if that is enough
    if (b->size < aligned && b->next && b->next->magic == BLOCK_FREE &&
        b->size + HEADER_SIZE + b->next->size >= aligned)
        block_merge_next(b);

    if (b->size >= aligned) {
        block_split(b, aligned);

        // A split may leave a free block next to another free block
        if (b->next && b->next->magic == BLOCK_FREE &&
            b->next->next && b->next->next->magic == BLOCK_FREE)
            block_merge_next(b->next);

        heap_unlock(flags);
        return ptr;
    }

    size_t old_size = b->size;
    heap_unlock(flags);

    void *new_ptr = kmalloc(size);
    if (new_ptr == NULL)
        return NULL;  // the original block is left untouched

    memcpy(new_ptr, ptr, old_size);
    kfree(ptr);
    return new_ptr;
}

void kheap_get_stats(kheap_stats_t *stats)
{
    memset(stats, 0, sizeof(*stats));

    uintptr_t flags = heap_lock();

    stats->heap_size = heap_end - HEAP_START;

    for (block_t *b = heap_first; b; b = b->next) {
        if (b->magic == BLOCK_USED) {
            stats->used_bytes += b->size;
            stats->used_blocks++;
        } else {
            stats->free_bytes += b->size;
            stats->free_blocks++;
        }
    }

    heap_unlock(flags);
}

void kheap_dump(void)
{
    uintptr_t flags = heap_lock();

    int i = 0;
    for (block_t *b = heap_first; b; b = b->next, i++) {
        if (i == 32) {
            printk("  ...\n");
            break;
        }
        printk("  %p  %u bytes  %s\n", block_data(b), (uint32_t)b->size,
               b->magic == BLOCK_USED ? "used" : "free");
    }

    heap_unlock(flags);
}

int kheap_check(void)
{
    uintptr_t flags = heap_lock();
    int ok = 1;

    block_t *prev = NULL;
    uintptr_t expected = HEAP_START;

    for (block_t *b = heap_first; b; b = b->next) {
        if ((uintptr_t)b != expected) {
            printk("heap check: block %p should start at %p\n", (void *)b, (void *)expected);
            ok = 0;
            break;
        }
        if (b->magic != BLOCK_USED && b->magic != BLOCK_FREE) {
            printk("heap check: bad magic at %p\n", (void *)b);
            ok = 0;
            break;
        }
        if (b->prev != prev) {
            printk("heap check: bad prev link at %p\n", (void *)b);
            ok = 0;
        }
        if (prev && prev->magic == BLOCK_FREE && b->magic == BLOCK_FREE) {
            printk("heap check: unmerged free blocks at %p\n", (void *)b);
            ok = 0;
        }

        expected = (uintptr_t)block_data(b) + b->size;
        prev = b;
    }

    if (ok && (prev != heap_last || expected != heap_end)) {
        printk("heap check: heap ends at %p, expected %p\n", (void *)expected, (void *)heap_end);
        ok = 0;
    }

    heap_unlock(flags);
    return ok;
}
