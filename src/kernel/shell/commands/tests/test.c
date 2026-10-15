// test.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-14


#include <kernel/config.h>

#ifdef CONFIG_CMD_TESTS

#include <kernel/shell/commands.h>

#include <kernel/printk.h>
// TODO(x86_64): drop the guards once the heap is ported
#if defined(__i386__)
#include <kernel/mm/heap.h>
#include <kernel/mm/pmm.h>
#endif
#include <string.h>
#include <stdint.h>


#if defined(__i386__)
#define HEAP_TEST_SLOTS 64

// Random alloc/realloc/free mix that checks contents and heap invariants,
// then verifies that freeing everything returns all memory
static void test_heap(void)
{
    static uint8_t *ptrs[HEAP_TEST_SLOTS];
    static size_t sizes[HEAP_TEST_SLOTS];
    uint32_t seed = 12345;
    int errors = 0;

    kheap_stats_t before;
    kheap_get_stats(&before);
    uint32_t pmm_before = pmm_free_pages();

    memset(ptrs, 0, sizeof(ptrs));

    for (int i = 0; i < 5000 && errors == 0; i++) {
        seed = seed * 1103515245 + 12345;
        int slot = (seed >> 16) % HEAP_TEST_SLOTS;
        seed = seed * 1103515245 + 12345;
        size_t size = 1 + (seed >> 16) % ((seed & 0x100) ? 65536 : 512);

        if (ptrs[slot]) {
            for (size_t j = 0; j < sizes[slot]; j++) {
                if (ptrs[slot][j] != (uint8_t)slot) {
                    printk("heap test: slot %d corrupted at byte %u\n", slot, (uint32_t)j);
                    errors++;
                    break;
                }
            }

            if (seed & 0x200) {
                uint8_t *p = krealloc(ptrs[slot], size);
                if (p == NULL) {
                    printk("heap test: krealloc failed\n");
                    errors++;
                    break;
                }
                // krealloc keeps the old contents up to the smaller size
                size_t kept = size < sizes[slot] ? size : sizes[slot];
                for (size_t j = 0; j < kept; j++) {
                    if (p[j] != (uint8_t)slot) {
                        printk("heap test: krealloc lost data in slot %d\n", slot);
                        errors++;
                        break;
                    }
                }
                memset(p, slot, size);
                ptrs[slot] = p;
                sizes[slot] = size;
            } else {
                kfree(ptrs[slot]);
                ptrs[slot] = NULL;
            }
        } else {
            ptrs[slot] = kmalloc(size);
            if (ptrs[slot] == NULL) {
                printk("heap test: kmalloc(%u) failed\n", (uint32_t)size);
                errors++;
                break;
            }
            memset(ptrs[slot], slot, size);
            sizes[slot] = size;
        }

        if (i % 250 == 0 && !kheap_check())
            errors++;
    }

    kheap_stats_t peak;
    kheap_get_stats(&peak);

    for (int slot = 0; slot < HEAP_TEST_SLOTS; slot++) {
        kfree(ptrs[slot]);
        ptrs[slot] = NULL;
    }

    uint32_t *zeros = kcalloc(256, sizeof(uint32_t));
    for (int i = 0; zeros && i < 256; i++) {
        if (zeros[i] != 0) {
            printk("heap test: kcalloc memory not zeroed\n");
            errors++;
            break;
        }
    }
    kfree(zeros);

    kheap_stats_t after;
    kheap_get_stats(&after);

    if (!kheap_check())
        errors++;

    if (after.used_blocks != before.used_blocks || after.heap_size != before.heap_size) {
        printk("heap test: memory not fully returned (%u blocks, %u KB)\n",
               (uint32_t)after.used_blocks, (uint32_t)(after.heap_size / 1024));
        errors++;
    }

    if (pmm_free_pages() != pmm_before) {
        printk("heap test: PMM leaked %u pages\n", pmm_before - pmm_free_pages());
        errors++;
    }

    printk("heap test: peak heap %u KB, %s\n", (uint32_t)(peak.heap_size / 1024),
           errors ? "FAILED" : "passed");
}
#endif

void cmd_test(int argc, char **argv)
{
    if (argc < 2)
    {
        printk("Usage: test <thing>\n");
        return;
    }

    if (strcmp(argv[1], "interrupt") == 0)
    {
        printk("Testing interrupt...\n");
        __asm__ volatile("int $0x3");
    }
    else if (strcmp(argv[1], "printk") == 0)
    {
        printk("printk works!\n");
    }
#if defined(__i386__)
    else if (strcmp(argv[1], "heap") == 0)
    {
        test_heap();
    }
#endif
    else if (strcmp(argv[1], "pagefault") == 0)
    {
        volatile uint32_t *ptr = (uint32_t*)0xDEADBEEF;
        *ptr = 0x1234;
    }
    else
    {
        printk("Unknown test\n");
    }
}

SHELL_CMD("tests", "test", "Kernel test command", cmd_test);

#endif // CONFIG_CMD_TESTS
