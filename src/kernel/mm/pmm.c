// pmm.c
// <kernel/mm/pmm.h>
// Author: Stre4k
// Date: 2026-03-08

#include <kernel/bitmap.h>
#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <stdint.h>
#include <string.h>

#define PAGE_SIZE 4096
#define MAX_MEMORY (1024 * 1024 * 1024)  // 1GB
#define MAX_PAGES  (MAX_MEMORY / PAGE_SIZE)

#define ALIGN_DOWN(x) ((x) & ~(uint64_t)(PAGE_SIZE - 1))
#define ALIGN_UP(x)   ALIGN_DOWN((x) + PAGE_SIZE - 1)

#define MB_FLAG_CMDLINE     (1 << 2)
#define MB_FLAG_MODS        (1 << 3)
#define MB_FLAG_MMAP        (1 << 6)
#define MB_FLAG_LOADER_NAME (1 << 9)

#define MAX_RESERVED 32

/* bit set = page in use */
static uint8_t bitmap[MAX_PAGES / 8];
/* bit set = page is usable RAM that may be handed out and freed */
static uint8_t usable[MAX_PAGES / 8];

static uint32_t total_pages;

extern uint32_t kernel_end;

typedef struct {
    uint64_t start;
    uint64_t end;
} range_t;

static range_t reserved[MAX_RESERVED];
static int reserved_count;

static void reserve(uint64_t start, uint64_t end) {
    if (end <= start)
        return;
    if (reserved_count == MAX_RESERVED)
        panic("pmm: too many reserved ranges");

    /* round outward so partly used pages stay reserved */
    reserved[reserved_count].start = ALIGN_DOWN(start);
    reserved[reserved_count].end = ALIGN_UP(end);
    reserved_count++;
}

static void reserve_string(uint32_t addr) {
    if (addr)
        reserve(addr, addr + strlen((const char *)(uintptr_t)addr) + 1);
}

/* Collect everything the PMM must never hand out */
static void reserve_boot_data(multiboot_info_t *mbi) {
    /* low 1MB (IVT, BDA, EBDA, BIOS) and the kernel image */
    reserve(0, (uintptr_t)&kernel_end);

    reserve((uintptr_t)mbi, (uintptr_t)mbi + sizeof(*mbi));
    reserve(mbi->mmap_addr, (uint64_t)mbi->mmap_addr + mbi->mmap_length);

    if (mbi->flags & MB_FLAG_CMDLINE)
        reserve_string(mbi->cmdline);

    if (mbi->flags & MB_FLAG_LOADER_NAME)
        reserve_string(mbi->boot_loader_name);

    if (mbi->flags & MB_FLAG_MODS) {
        multiboot_mod_t *mods = (multiboot_mod_t *)(uintptr_t)mbi->mods_addr;

        reserve(mbi->mods_addr,
                (uint64_t)mbi->mods_addr + mbi->mods_count * sizeof(multiboot_mod_t));

        for (uint32_t i = 0; i < mbi->mods_count; i++) {
            reserve(mods[i].mod_start, mods[i].mod_end);
            reserve_string(mods[i].cmdline);
        }
    }
}

static int is_reserved(uint64_t page_addr) {
    for (int i = 0; i < reserved_count; i++) {
        if (page_addr >= reserved[i].start && page_addr < reserved[i].end)
            return 1;
    }
    return 0;
}

void pmm_init(multiboot_info_t *mbi) {
    if (mbi == NULL)
        panic("pmm: no multiboot info");

    if (!(mbi->flags & MB_FLAG_MMAP))
        panic("pmm: bootloader did not provide a memory map");

    memset(bitmap, 0xFF, sizeof(bitmap));
    memset(usable, 0, sizeof(usable));
    total_pages = 0;
    reserved_count = 0;

    reserve_boot_data(mbi);

    uint32_t addr = mbi->mmap_addr;
    uint32_t end = addr + mbi->mmap_length;

    /* Pass 1: free every whole page inside a usable region */
    while (addr < end) {
        multiboot_mmap_entry_t *entry =
            (multiboot_mmap_entry_t *)(uintptr_t)addr;

        /* size excludes the size field itself; a short entry means a corrupt map */
        if (entry->size < sizeof(*entry) - sizeof(entry->size))
            break;

        if (entry->type == 1) {
            /* round inward: a page is usable only if it lies fully in the region */
            uint64_t start = ALIGN_UP(entry->addr);
            uint64_t stop = ALIGN_DOWN(entry->addr + entry->len);

            if (stop > MAX_MEMORY)
                stop = MAX_MEMORY;

            for (uint64_t page = start; page < stop; page += PAGE_SIZE) {
                if (is_reserved(page))
                    continue;

                uint32_t index = page / PAGE_SIZE;
                if (!test_bit(usable, index)) {
                    set_bit(usable, index);
                    clear_bit(bitmap, index);
                    total_pages++;
                }
            }
        }

        addr += entry->size + sizeof(entry->size);
    }

    /* Pass 2: entries may overlap, so non-usable regions win over usable ones */
    addr = mbi->mmap_addr;
    while (addr < end) {
        multiboot_mmap_entry_t *entry =
            (multiboot_mmap_entry_t *)(uintptr_t)addr;

        if (entry->size < sizeof(*entry) - sizeof(entry->size))
            break;

        if (entry->type != 1) {
            uint64_t start = ALIGN_DOWN(entry->addr);
            uint64_t stop = ALIGN_UP(entry->addr + entry->len);

            if (stop > MAX_MEMORY)
                stop = MAX_MEMORY;

            for (uint64_t page = start; page < stop; page += PAGE_SIZE) {
                uint32_t index = page / PAGE_SIZE;
                if (test_bit(usable, index)) {
                    clear_bit(usable, index);
                    set_bit(bitmap, index);
                    total_pages--;
                }
            }
        }

        addr += entry->size + sizeof(entry->size);
    }
}

void *pmm_alloc_page() {
    for (uint32_t i = 0; i < MAX_PAGES; i++) {
        if (!test_bit(bitmap, i)) {
            set_bit(bitmap, i);
            return (void *)(uintptr_t)(i * PAGE_SIZE);
        }
    }
    return NULL;
}

void pmm_free_page(void *page) {
    uintptr_t addr = (uintptr_t)page;

    if (addr & (PAGE_SIZE - 1))
        panic("pmm_free_page: address is not page aligned");

    uint32_t index = addr / PAGE_SIZE;

    if (index >= MAX_PAGES || !test_bit(usable, index))
        panic("pmm_free_page: address is not usable RAM");

    if (!test_bit(bitmap, index))
        panic("pmm_free_page: double free");

    clear_bit(bitmap, index);
}

/* Number of usable RAM pages managed by the PMM */
uint32_t pmm_total_pages() {
    return total_pages;
}

uint32_t pmm_free_pages() {
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_PAGES; i++) {
        if (!test_bit(bitmap, i))
            count++;
    }
    return count;
}
