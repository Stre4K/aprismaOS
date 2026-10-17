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
#include <kernel/proc.h>
#ifdef CONFIG_USERMODE
#include <kernel/arch/memlayout.h>
#endif
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

    if (after.used_blocks != before.used_blocks || after.used_bytes != before.used_bytes ||
        after.heap_size > before.heap_size) {
        printk("heap test: memory not fully returned (%u blocks, %u KB)\n",
               (uint32_t)after.used_blocks, (uint32_t)(after.heap_size / 1024));
        errors++;
    }

    // The heap may end up smaller than it started (a free tail left by
    // earlier work can be trimmed now), but every page has to add up
    uint32_t heap_pages_before = (uint32_t)(before.heap_size / 4096);
    uint32_t heap_pages_after = (uint32_t)(after.heap_size / 4096);
    if (pmm_free_pages() + heap_pages_after != pmm_before + heap_pages_before) {
        printk("heap test: PMM leaked %d pages\n",
               (int)(pmm_before + heap_pages_before) - (int)(pmm_free_pages() + heap_pages_after));
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

#ifdef CONFIG_USERMODE
/* ---- test user ---- */

// Run a program to the end; returns its exit code (-1000 if it could not start)
static int run_program(const char *name, bool *killed)
{
    process_t *p = process_spawn(name);
    if (p == NULL)
        return -1000;
    return process_wait_info(p, killed);
}

static bool expect_exit(const char *name, int expected)
{
    bool killed;
    int code = run_program(name, &killed);
    if (killed || code != expected) {
        printk("user test: %s %s %d, expected exit code %d\n", name,
               killed ? "was killed, code" : "exited with", code, expected);
        return false;
    }
    return true;
}

static bool expect_killed(const char *name)
{
    bool killed;
    int code = run_program(name, &killed);
    if (!killed) {
        printk("user test: %s should have been killed, exited with %d\n", name, code);
        return false;
    }
    return true;
}

// Run heaptest's out-of-memory check while the kernel holds all but a
// few free pages (enough to start the program, not for 1 MiB of heap)
static bool heap_out_of_memory(void)
{
    enum { LEAVE_FREE = 96 };
    void *hoard = NULL;  // held pages, linked through their first word
    while (pmm_free_pages() > LEAVE_FREE) {
        void *page = pmm_alloc_page();
        if (page == NULL)
            break;
        *(void **)P2V(page) = hoard;
        hoard = page;
    }

    const char *argv[] = { "heaptest", "oom" };
    process_t *p = process_spawn_args("heaptest", 2, argv, NULL);
    bool killed = false;
    int code = p ? process_wait_info(p, &killed) : -1000;

    while (hoard) {
        void *next = *(void **)P2V(hoard);
        pmm_free_page(hoard);
        hoard = next;
    }

    if (killed || code != 0) {
        printk("user test: heaptest oom %s %d\n", killed ? "was killed, code" : "exited with", code);
        return false;
    }
    return true;
}

// Ctrl+C must reach a process waiting on a pipe that nothing else will
// touch: here it waits to write into a full pipe only it could read
static bool interrupt_wakes_pipe_writer(void)
{
    const char *argv[] = { "pipes", "block" };
    process_t *p = process_spawn_args("pipes", 2, argv, NULL);
    if (p == NULL)
        return false;
    thread_sleep(100);  // let it fill the pipe and wait
    process_interrupt_group(p->pgid);
    for (int i = 0; i < 100 && !p->exited; i++)
        thread_sleep(10);
    if (!p->exited) {
        printk("user test: Ctrl+C did not wake a process waiting on a pipe\n");
        return false;  // it stays stuck; only happens when this is broken
    }
    bool killed;
    process_wait_info(p, &killed);
    if (!killed)
        printk("user test: pipes block was not killed\n");
    return killed;
}

// Ctrl+C must also end a process in the middle of a long sleep
static bool interrupt_wakes_sleeper(void)
{
    const char *argv[] = { "sleep", "100" };
    process_t *p = process_spawn_args("sleep", 2, argv, NULL);
    if (p == NULL)
        return false;
    thread_sleep(100);  // let it start sleeping
    process_interrupt_group(p->pgid);
    for (int i = 0; i < 100 && !p->exited; i++)
        thread_sleep(10);
    if (!p->exited) {
        printk("user test: Ctrl+C did not wake a sleeping process\n");
        return false;  // it ends by itself after 100 s
    }
    bool killed;
    process_wait_info(p, &killed);
    return killed;
}

static void test_user(void)
{
    int errors = 0;

    // Exit codes and system call error handling
    errors += !expect_exit("exitcode", 42);
    errors += !expect_exit("badptr", 0);
    errors += !expect_exit("files", 0);  // open/read/close on the ramdisk

    // Arguments: args exits with argc (100+ means a broken stack layout)
    static const char *const three[] = { "args", "alpha", "beta", "gamma" };
    static const char *many[PROCESS_MAX_ARGS + 1];
    static char long_arg[PROCESS_MAX_ARG_BYTES];
    for (int i = 0; i <= PROCESS_MAX_ARGS; i++)
        many[i] = "x";
    memset(long_arg, 'a', sizeof(long_arg) - 1);
    const char *too_long[] = { "args", long_arg };

    process_t *with_args = process_spawn_args("args", 4, three, NULL);
    int args_code = with_args ? process_wait(with_args) : -1;
    process_t *max_args = process_spawn_args("args", PROCESS_MAX_ARGS, many, NULL);
    int max_code = max_args ? process_wait(max_args) : -1;
    process_t *over = process_spawn_args("args", PROCESS_MAX_ARGS + 1, many, NULL);
    process_t *over_bytes = process_spawn_args("args", 2, too_long, NULL);
    if (args_code != 4 || max_code != PROCESS_MAX_ARGS || over || over_bytes) {
        printk("user test: arguments wrong (exit %d and %d, too many %s, too long %s)\n", args_code,
               max_code, over ? "accepted" : "refused", over_bytes ? "accepted" : "refused");
        errors++;
        if (over)
            process_wait(over);
        if (over_bytes)
            process_wait(over_bytes);
    }

    // Processes starting processes: spawn, wait and readdir from user mode
    errors += !expect_exit("spawner", 0);

    // The writable filesystem
    errors += !expect_exit("fstest", 0);

    // Pipes, descriptors and spawn options
    errors += !expect_exit("pipes", 0);
    errors += !interrupt_wakes_pipe_writer();
    errors += !interrupt_wakes_sleeper();

    // The heap: brk and malloc, and running out of memory
    errors += !expect_exit("heaptest", 0);
    errors += !heap_out_of_memory();

    // Faults in user mode end the process, not the kernel
    errors += !expect_killed("crash");
    errors += !expect_killed("privileged");

    // Two copies at once: each sees only its own memory at the same address
    process_t *a = process_spawn("isolation");
    process_t *b = process_spawn("isolation");
    if (a == NULL || b == NULL) {
        printk("user test: cannot start two processes\n");
        errors++;
    }
    int code_a = a ? process_wait(a) : -1;
    int code_b = b ? process_wait(b) : -1;
    if (code_a != 0 || code_b != 0) {
        printk("user test: isolation failed (%d, %d)\n", code_a, code_b);
        errors++;
    }

    // x87/SSE registers belong to each process: two copies fill them with
    // different values and rounding modes while switching back and forth
    process_t *f1 = process_spawn("fpu");
    process_t *f2 = process_spawn("fpu");
    int fpu1 = f1 ? process_wait(f1) : -1;
    int fpu2 = f2 ? process_wait(f2) : -1;
    if (fpu1 != 0 || fpu2 != 0) {
        printk("user test: fpu state was not kept (%d, %d)\n", fpu1, fpu2);
        errors++;
    }

    // User code is preemptible: this thread keeps waking up while a
    // process busy-loops without making system calls
    process_t *spinner = process_spawn("spin");
    int wakeups = 0;
    while (spinner && !spinner->exited) {
        thread_sleep(5);
        wakeups++;
    }
    if (spinner == NULL || process_wait(spinner) != 0 || wakeups < 3) {
        printk("user test: kernel ran only %d times while spin was busy\n", wakeups);
        errors++;
    }

    // Exited processes leave nothing behind
    thread_sleep(50);  // let the scheduler free the processes above
    size_t threads_before = thread_count();
    uint32_t pages_before = pmm_free_pages();
    kheap_stats_t heap_before;
    kheap_get_stats(&heap_before);

    for (int i = 0; i < 20; i++)
        errors += !expect_exit("exitcode", 42);
    for (int i = 0; i < 3; i++)
        errors += !expect_exit("spawner", 0);  // also leaves a child behind each time
    errors += !expect_exit("heaptest", 0);     // its heap pages must come back
    errors += !heap_out_of_memory();           // and those of a brk that failed halfway
    errors += !expect_exit("pipes", 0);        // pipes and open files must be freed
    errors += !expect_exit("fstest", 0);       // and files made and removed in /tmp
    thread_sleep(50);

    kheap_stats_t heap_after;
    kheap_get_stats(&heap_after);
    if (thread_count() != threads_before || pmm_free_pages() != pages_before ||
        heap_after.used_blocks != heap_before.used_blocks) {
        printk("user test: processes leaked (threads %u -> %u, free pages %u -> %u, heap blocks %u -> %u)\n",
               (unsigned)threads_before, (unsigned)thread_count(), pages_before, pmm_free_pages(),
               (unsigned)heap_before.used_blocks, (unsigned)heap_after.used_blocks);
        errors++;
    }

    printk("user test: %d wakeups during spin, %s\n", wakeups, errors ? "FAILED" : "passed");
}
#endif // CONFIG_USERMODE

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
#ifdef CONFIG_USERMODE
    else if (strcmp(argv[1], "user") == 0)
    {
        test_user();
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
