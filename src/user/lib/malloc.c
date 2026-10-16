// malloc.c
// Memory allocation for user programs: a first-fit allocator on top of
// sbrk(), built like the kernel heap. Every block (used or free) starts
// with a header, and all blocks are kept in one address-ordered list, so a
// freed block merges with free neighbours. The heap grows with sbrk() and
// gives whole pages back when a large free block sits at its end.
//
// Programs may call sbrk() themselves too; the allocator then just sees a
// gap and only merges blocks that really touch.

#include "ulib.h"

#include <stdint.h>

#define ALIGN      16
#define PAGE       4096
#define GROW_MIN   (64 * 1024)   // ask sbrk for at least this much at a time
#define TRIM_MIN   (128 * 1024)  // give back a free tail bigger than this...
#define TRIM_KEEP  GROW_MIN      // ...keeping this much of it
#define MAX_SIZE   ((size_t)1 << 40)  // larger requests fail without trying

#define BLOCK_USED 0xA110CA7EU
#define BLOCK_FREE 0xF4EEB10CU

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((size_t)(a) - 1))

typedef struct block {
    unsigned magic;          // BLOCK_USED or BLOCK_FREE, anything else is corruption
    size_t size;             // payload bytes (a multiple of ALIGN), excluding the header
    struct block *prev;      // previous block by address
    struct block *next;      // next block by address
} block_t;

#define HEADER    ALIGN_UP(sizeof(block_t), ALIGN)
#define MIN_SPLIT (HEADER + ALIGN)

static block_t *first;
static block_t *last;

static void *payload(block_t *b)
{
    return (char *)b + HEADER;
}

static char *end_of(block_t *b)
{
    return (char *)b + HEADER + b->size;
}

// True if b's next block starts right where b ends (no foreign sbrk between)
static int touches_next(block_t *b)
{
    return b->next && end_of(b) == (char *)b->next;
}

__attribute__((noreturn)) static void corrupted(const char *what)
{
    print("malloc: ");
    print(what);
    print("\n");
    exit(127);
}

static block_t *header_of(void *ptr)
{
    block_t *b = (block_t *)((char *)ptr - HEADER);
    if (((uintptr_t)ptr & (ALIGN - 1)) || b->magic != BLOCK_USED)
        corrupted(b->magic == BLOCK_FREE ? "double free" : "invalid pointer (or heap corruption)");
    return b;
}

// Insert b into the list after `after` (NULL: at the front)
static void link_after(block_t *after, block_t *b)
{
    b->prev = after;
    b->next = after ? after->next : first;
    if (b->next)
        b->next->prev = b;
    else
        last = b;
    if (after)
        after->next = b;
    else
        first = b;
}

static void unlink_block(block_t *b)
{
    if (b->prev)
        b->prev->next = b->next;
    else
        first = b->next;
    if (b->next)
        b->next->prev = b->prev;
    else
        last = b->prev;
}

// Absorb the next block into b if it is free and touches b
static void merge_next(block_t *b)
{
    if (touches_next(b) && b->next->magic == BLOCK_FREE) {
        block_t *next = b->next;
        b->size += HEADER + next->size;
        unlink_block(next);
        next->magic = 0;
    }
}

// Merge a free block with free neighbours that touch it; returns the result
static block_t *merge(block_t *b)
{
    merge_next(b);
    if (b->prev && b->prev->magic == BLOCK_FREE && touches_next(b->prev)) {
        block_t *prev = b->prev;
        prev->size += HEADER + b->size;
        unlink_block(b);
        b->magic = 0;
        b = prev;
    }
    return b;
}

// Cut b down to `size` bytes if the rest is big enough for another block.
// The rest only merges forward: b itself may still be marked free.
static void split(block_t *b, size_t size)
{
    if (b->size < size + MIN_SPLIT)
        return;
    block_t *rest = (block_t *)((char *)payload(b) + size);
    rest->magic = BLOCK_FREE;
    rest->size = b->size - size - HEADER;
    b->size = size;
    link_after(b, rest);
    merge_next(rest);
}

// Get at least `size` bytes of free block from sbrk; NULL if it says no
static block_t *grow(size_t size)
{
    char *brk_now = sbrk(0);

    // A free block at the end of the heap just gets bigger
    if (last && last->magic == BLOCK_FREE && end_of(last) == brk_now) {
        size_t more = ALIGN_UP(size - last->size, PAGE);
        if (more < GROW_MIN)
            more = GROW_MIN;
        if (sbrk((long)more) == (void *)-1)
            return NULL;
        last->size += more;
        return last;
    }

    // If someone else left the break unaligned, the block starts `pad`
    // bytes later; asking for `pad` more keeps its end at the new break
    size_t pad = (size_t)(-(uintptr_t)brk_now & (ALIGN - 1));
    size_t more = ALIGN_UP(HEADER + size, PAGE);
    if (more < GROW_MIN)
        more = GROW_MIN;
    char *mem = sbrk((long)(more + pad));
    if (mem == (void *)-1)
        return NULL;

    block_t *b = (block_t *)(mem + pad);
    b->magic = BLOCK_FREE;
    b->size = more - HEADER;
    link_after(last, b);
    return merge(b);
}

// Give whole pages of a large free block at the end back to the kernel
static void trim(void)
{
    if (!last || last->magic != BLOCK_FREE || last->size <= TRIM_MIN || end_of(last) != (char *)sbrk(0))
        return;
    size_t release = (last->size - TRIM_KEEP) & ~(size_t)(PAGE - 1);
    if (sbrk(-(long)release) != (void *)-1)
        last->size -= release;
}

void *malloc(size_t size)
{
    if (size > MAX_SIZE)
        return NULL;
    size = size ? ALIGN_UP(size, ALIGN) : ALIGN;

    block_t *b;
    for (b = first; b; b = b->next) {
        if (b->magic == BLOCK_FREE && b->size >= size)
            break;
    }
    if (b == NULL && (b = grow(size)) == NULL)
        return NULL;

    split(b, size);
    b->magic = BLOCK_USED;
    return payload(b);
}

void free(void *ptr)
{
    if (ptr == NULL)
        return;
    block_t *b = header_of(ptr);
    b->magic = BLOCK_FREE;
    merge(b);
    trim();
}

void *calloc(size_t count, size_t size)
{
    if (size && count > MAX_SIZE / size)
        return NULL;  // count * size would overflow (or is absurd)
    void *ptr = malloc(count * size);
    if (ptr)
        memset(ptr, 0, count * size);  // reused blocks are not zeroed
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    if (size > MAX_SIZE)
        return NULL;

    block_t *b = header_of(ptr);
    size_t wanted = ALIGN_UP(size, ALIGN);

    // Grow in place into a free block right after this one
    if (wanted > b->size && touches_next(b) && b->next->magic == BLOCK_FREE &&
        b->size + HEADER + b->next->size >= wanted)
        merge_next(b);
    if (wanted <= b->size) {
        split(b, wanted);
        return ptr;
    }

    void *moved = malloc(size);
    if (moved == NULL)
        return NULL;  // the old block stays valid
    memcpy(moved, ptr, b->size);
    free(ptr);
    return moved;
}

void malloc_stats(malloc_stats_t *stats)
{
    stats->used_bytes = stats->free_bytes = 0;
    stats->used_blocks = stats->free_blocks = 0;
    for (block_t *b = first; b; b = b->next) {
        if (b->magic == BLOCK_USED) {
            stats->used_bytes += b->size;
            stats->used_blocks++;
        } else {
            stats->free_bytes += b->size;
            stats->free_blocks++;
        }
    }
}

int malloc_check(void)
{
    int problems = 0;
    block_t *prev = NULL;
    for (block_t *b = first; b; prev = b, b = b->next) {
        if (b->magic != BLOCK_USED && b->magic != BLOCK_FREE)
            return problems + 1;  // cannot follow the list any further
        if (((uintptr_t)payload(b) & (ALIGN - 1)) || (b->size & (ALIGN - 1)))
            problems++;           // misaligned
        if (b->prev != prev || (prev && (char *)b < end_of(prev)))
            problems++;           // broken links, or overlapping blocks
        if (prev && prev->magic == BLOCK_FREE && b->magic == BLOCK_FREE && touches_next(prev))
            problems++;           // two touching free blocks were not merged
    }
    if (prev != last)
        problems++;
    return problems;
}
