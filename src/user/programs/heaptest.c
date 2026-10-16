// heaptest: checks the brk system call and malloc (run by the kernel's
// `test user`). Exits with the number of failed checks.
//
//   heaptest         all checks
//   heaptest oom     the kernel left little memory: growing must fail cleanly
//   heaptest touch   use heap memory after giving it back (must be killed)
//   heaptest twice   free a block twice (must exit with 127)
#include "ulib.h"

#define PAGE           4096UL
#define HEAP_MAX       0x4000000UL  // USER_HEAP_MAX, 64 MiB
#define KERNEL_ADDRESS 0xFFFFFFFF80000000UL

int main(int argc, char **argv);

static int failures;
static char in_bss[100];  // the heap starts after the program, so after this

static void check(int ok, const char *what)
{
    if (!ok) {
        print("heaptest: failed: ");
        print(what);
        print("\n");
        failures++;
    }
}

static int all_zero(const char *p, unsigned long n)
{
    for (unsigned long i = 0; i < n; i++)
        if (p[i])
            return 0;
    return 1;
}

static void check_brk(void)
{
    char *start = (char *)brk(0);
    check((unsigned long)start % PAGE == 0, "the heap starts on a page");
    check(start >= in_bss + sizeof(in_bss), "the heap starts after the program");
    check(sbrk(0) == start, "sbrk(0) is the current end");

    // Grow: new memory is zeroed and writable
    check(brk(start + 3 * PAGE + 100) == (long)(start + 3 * PAGE + 100), "grow by three pages and a bit");
    check(all_zero(start, 3 * PAGE + 100), "new heap memory is zeroed");
    memset(start, 0x77, 3 * PAGE + 100);

    // Shrink, then grow again: what comes back is zeroed again
    check(brk(start + 50) == (long)(start + 50), "shrink");
    check(brk(start + PAGE) == (long)(start + PAGE), "grow again");
    check(start[0] == 0x77 && start[49] == 0x77, "memory below the end is kept");
    check(all_zero(start + 50, PAGE - 50), "memory handed out again is zeroed");

    // Out of range: the heap stays as it was
    check(brk(start - 1) == -ENOMEM, "below the start of the heap");
    check(brk(start + HEAP_MAX + 1) == -ENOMEM, "past the 64 MiB limit");
    check(brk((void *)KERNEL_ADDRESS) == -ENOMEM, "into kernel memory");
    check(brk((void *)1) == -ENOMEM, "near address 0");
    check(sbrk(-(long)(2 * HEAP_MAX)) == (void *)-1, "sbrk below the start");
    check(sbrk(0) == start + PAGE, "failed calls leave the heap alone");

    // The whole 64 MiB at once, and back
    check(brk(start + HEAP_MAX) == (long)(start + HEAP_MAX), "grow to the limit");
    check(all_zero(start + PAGE, 3 * PAGE), "pages given back and taken again are zeroed");
    start[HEAP_MAX - 1] = 1;
    check(brk(start) == (long)start, "shrink to nothing");
}

static void check_malloc(void)
{
    // Many blocks of different sizes, each with its own pattern
    enum { COUNT = 500 };
    static unsigned char *blocks[COUNT];
    for (int i = 0; i < COUNT; i++) {
        unsigned long size = 1 + (unsigned long)(i * 37) % 2000;
        blocks[i] = malloc(size);
        check(blocks[i] != 0 && (unsigned long)blocks[i] % 16 == 0, "malloc returns aligned memory");
        if (blocks[i])
            memset(blocks[i], i & 0xFF, size);
    }
    for (int i = 0; i < COUNT; i += 2) {
        free(blocks[i]);
        blocks[i] = 0;
    }
    for (int i = 1; i < COUNT; i += 2) {
        unsigned long size = 1 + (unsigned long)(i * 37) % 2000;
        int intact = 1;
        for (unsigned long j = 0; j < size; j++)
            intact &= blocks[i][j] == (i & 0xFF);
        check(intact, "blocks keep their contents");
        blocks[i] = realloc(blocks[i], size * 3);
        check(blocks[i] != 0 && blocks[i][size - 1] == (i & 0xFF), "realloc keeps the contents");
    }
    for (int i = 0; i < COUNT; i++)
        free(blocks[i]);
    check(malloc_check() == 0, "the heap's bookkeeping is consistent");

    malloc_stats_t stats;
    malloc_stats(&stats);
    check(stats.used_blocks == 0, "everything was freed");

    // A big block grows the heap; freeing it gives the memory back
    char *before = sbrk(0);
    char *big = malloc(8 << 20);
    check(big != 0 && (char *)sbrk(0) >= before + (7 << 20), "a big block grows the heap");
    if (big)
        big[(8 << 20) - 1] = 1;
    free(big);
    check((char *)sbrk(0) < before + (256 << 10), "freeing it shrinks the heap");

    unsigned char *zeroed = calloc(1000, 4);
    check(zeroed && all_zero((char *)zeroed, 4000), "calloc zeroes");
    free(zeroed);

    check(malloc(HEAP_MAX) == 0, "more than the heap limit fails");
    check(malloc_check() == 0, "and leaves the heap consistent");
}

// The kernel left only a few free pages: growing by 1 MiB must fail
// without changing anything, and small allocations must still work
static int out_of_memory(void)
{
    char *end = sbrk(0);
    check(brk(end + (1 << 20)) == -ENOMEM, "brk fails when memory runs out");
    check(sbrk(0) == end, "a failed brk leaves the heap as it was");
    check(malloc(1 << 20) == 0, "malloc returns NULL");
    char *small = malloc(100);
    check(small != 0, "a small block still fits");
    free(small);
    return failures;
}

static int run_self(const char *mode, int *code)
{
    char *args[] = { "heaptest", (char *)mode, 0 };
    long pid = spawn("heaptest", args);
    return pid < 0 ? -1 : (int)wait((int)pid, code);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "oom") == 0)
        return out_of_memory();
    if (argc > 1 && strcmp(argv[1], "touch") == 0) {
        char *start = sbrk(2 * PAGE);
        start[PAGE] = 1;  // in use, so the TLB has it...
        brk(start);
        start[PAGE] = 2;  // ...but it is no longer mapped: the kernel kills us here
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "twice") == 0) {
        char *p = malloc(10);
        free(p);
        free(p);
        return 0;
    }

    check_brk();
    check_malloc();

    int code = 0;
    check(run_self("touch", &code) == WAIT_KILLED, "memory given back is unmapped");
    check(run_self("twice", &code) == 0 && code == 127, "a double free ends the program");

    print(failures ? "heaptest: FAILED\n" : "heaptest: ok\n");
    return failures;
}
