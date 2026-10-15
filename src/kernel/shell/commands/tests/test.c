// test.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-14


#include <kernel/config.h>

#ifdef CONFIG_CMD_TESTS

#include <kernel/shell/commands.h>

#include <kernel/printk.h>
#include <kernel/mm/heap.h>
#include <kernel/mm/pmm.h>
#include <kernel/arch/timer.h>
#include <kernel/sched.h>
#include <string.h>
#include <stdint.h>


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

// Busy-wait about half a second and check that the PIT kept ticking: shell
// commands must run with interrupts enabled
static void test_timer(void)
{
    uint64_t start = pit_get_ticks();
    for (volatile uint32_t spin = 0; spin < 200000000 && pit_get_ticks() - start < 50; spin++)
        ;
    uint64_t elapsed = pit_get_ticks() - start;
    printk("timer test: %llu ticks during the command, %s\n", elapsed,
           elapsed >= 50 ? "passed" : "FAILED (timer stopped)");
}

// Print more lines than the scrollback holds; the last ones must still
// appear in order at the bottom of the screen
static void test_scroll(void)
{
    for (int i = 0; i < 1500; i++)
        printk("scroll line %d\n", i);
    printk("scroll test done\n");
}

#ifdef CONFIG_SCHED
/* ---- test threads ---- */

static volatile uint64_t spin_count;
static volatile bool spin_stop;

// Never yields: it only gets the CPU if the scheduler preempts others
static void spinner(void *arg)
{
    (void)arg;
    while (!spin_stop)
        spin_count++;
}

static volatile int wake_order[3];
static volatile int wake_count;

static void sleeper(void *arg)
{
    int ms = (int)(intptr_t)arg;
    thread_sleep(ms);
    preempt_disable();
    wake_order[wake_count++] = ms;
    preempt_enable();
}

static volatile int workers_done;

static void worker(void *arg)
{
    (void)arg;
    __atomic_fetch_add(&workers_done, 1, __ATOMIC_SEQ_CST);
}

// Sleep in small steps until *value reaches target or `ms` pass
static bool wait_for_value(volatile int *value, int target, int ms)
{
    for (int waited = 0; *value < target && waited < ms; waited += 10)
        thread_sleep(10);
    return *value >= target;
}

static size_t thread_count(void)
{
    thread_info_t dummy;
    return thread_list(&dummy, 0);
}

static void test_threads(void)
{
    int errors = 0;

    // 1. Preemption: busy-wait here without yielding; a thread that also
    // never yields must still get CPU time
    spin_count = 0;
    spin_stop = false;
    if (!thread_create("spinner", spinner, NULL)) {
        printk("threads test: cannot create a thread\n");
        return;
    }
    uint64_t start = pit_get_ticks();
    while (pit_get_ticks() - start < 30)
        ;
    uint64_t spins = spin_count;
    spin_stop = true;
    if (spins == 0) {
        printk("threads test: the spinner never ran (no preemption)\n");
        errors++;
    }

    // 2. Sleeping threads wake in order of their sleep time
    wake_count = 0;
    thread_create("sleep60", sleeper, (void *)(intptr_t)60);
    thread_create("sleep20", sleeper, (void *)(intptr_t)20);
    thread_create("sleep40", sleeper, (void *)(intptr_t)40);
    if (!wait_for_value(&wake_count, 3, 2000)) {
        printk("threads test: only %d of 3 sleepers woke up\n", wake_count);
        errors++;
    } else if (wake_order[0] != 20 || wake_order[1] != 40 || wake_order[2] != 60) {
        printk("threads test: sleepers woke in order %d %d %d, expected 20 40 60\n",
               wake_order[0], wake_order[1], wake_order[2]);
        errors++;
    }

    // 3. Exited threads are freed: thread count, pages and heap return
    // to where they were. Give the threads above time to exit first.
    thread_sleep(50);
    size_t threads_before = thread_count();
    uint32_t pages_before = pmm_free_pages();
    kheap_stats_t heap_before;
    kheap_get_stats(&heap_before);

    workers_done = 0;
    for (int i = 0; i < 20; i++) {
        if (!thread_create("worker", worker, NULL)) {
            printk("threads test: cannot create worker %d\n", i);
            errors++;
            break;
        }
    }
    if (!wait_for_value(&workers_done, 20, 2000)) {
        printk("threads test: only %d of 20 workers ran\n", workers_done);
        errors++;
    }
    thread_sleep(50);  // let the scheduler free them

    kheap_stats_t heap_after;
    kheap_get_stats(&heap_after);
    if (thread_count() != threads_before || pmm_free_pages() != pages_before ||
        heap_after.used_blocks != heap_before.used_blocks) {
        printk("threads test: exited threads leaked (threads %u -> %u, free pages %u -> %u, heap blocks %u -> %u)\n",
               (unsigned)threads_before, (unsigned)thread_count(), pages_before, pmm_free_pages(),
               (unsigned)heap_before.used_blocks, (unsigned)heap_after.used_blocks);
        errors++;
    }

    printk("threads test: %llu spinner loops while preempted, %s\n",
           (unsigned long long)spins, errors ? "FAILED" : "passed");
}

// Recurse until the thread's stack runs into its guard page
static int stack_eater(int depth)
{
    volatile char buffer[256];
    buffer[0] = (char)depth;
    return depth > 0 ? stack_eater(depth - 1) + buffer[0] : 0;
}

static void overflow_thread(void *arg)
{
    (void)arg;
#if defined(__x86_64__)
    printk("stack overflow test: recursing, expect a double fault\n");
#else
    // i386 has no Interrupt Stack Table: the double fault handler would
    // need the overflowed stack too, so the CPU resets (triple fault)
    printk("stack overflow test: recursing, expect a reset (triple fault) on i386\n");
#endif
    stack_eater(1000000);
}
#endif // CONFIG_SCHED

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
#ifdef CONFIG_SCHED
    else if (strcmp(argv[1], "threads") == 0)
    {
        test_threads();
    }
    else if (strcmp(argv[1], "stackoverflow") == 0)
    {
        thread_create("overflow", overflow_thread, NULL);
        thread_sleep(500);
    }
#endif
    else if (strcmp(argv[1], "timer") == 0)
    {
        test_timer();
    }
    else if (strcmp(argv[1], "scroll") == 0)
    {
        test_scroll();
    }
    else if (strcmp(argv[1], "heap") == 0)
    {
        test_heap();
    }
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
