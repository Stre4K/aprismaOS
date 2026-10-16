// test_malloc.c
// The user programs' allocator: src/user/lib/malloc.c, running on a fake
// sbrk() over a static arena. The allocator keeps its state between tests,
// so every test frees what it allocates.

#include "aprisma.h"
#include "test.h"

#include <setjmp.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    size_t used_bytes, free_bytes;
    size_t used_blocks, free_blocks;
} aprisma_malloc_stats_t;

void *aprisma_malloc(size_t size);
void aprisma_free(void *ptr);
void *aprisma_calloc(size_t count, size_t size);
void *aprisma_realloc(void *ptr, size_t size);
void aprisma_malloc_stats(aprisma_malloc_stats_t *stats);
int aprisma_malloc_check(void);

/* ---- the runtime functions malloc.c uses ---- */

#define ARENA_SIZE (32u << 20)
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(4096)));
static size_t arena_brk;                  // bytes in use, like the program break
static size_t arena_limit = ARENA_SIZE;   // lower it to make sbrk fail

void *aprisma_sbrk(long increment)
{
    if (increment < 0 ? (size_t)-increment > arena_brk : (size_t)increment > arena_limit - arena_brk)
        return (void *)-1;
    void *old = arena + arena_brk;
    arena_brk += increment;
    return old;
}

static char message[256];
static jmp_buf exit_jump;
static int exit_code;

void aprisma_print(const char *s)
{
    strncat(message, s, sizeof(message) - strlen(message) - 1);
}

__attribute__((noreturn)) void aprisma_exit(int code)
{
    exit_code = code;
    longjmp(exit_jump, 1);
}

static size_t used_blocks(void)
{
    aprisma_malloc_stats_t stats;
    aprisma_malloc_stats(&stats);
    return stats.used_blocks;
}

/* ---- tests ---- */

TEST(malloc_returns_aligned_separate_blocks)
{
    char *a = aprisma_malloc(1);
    char *b = aprisma_malloc(100);
    char *c = aprisma_malloc(0);  // still a real, unique block
    CHECK(a && b && c);
    CHECK((uintptr_t)a % 16 == 0 && (uintptr_t)b % 16 == 0 && (uintptr_t)c % 16 == 0);
    CHECK(b >= a + 16 && c >= b + 100);
    CHECK(c - a < 4096);  // small blocks share the heap, not a region each
    memset(a, 1, 1);
    memset(b, 2, 100);
    CHECK(a[0] == 1 && b[99] == 2);
    CHECK_INT_EQ(used_blocks(), 3);
    aprisma_free(a);
    aprisma_free(b);
    aprisma_free(c);
    aprisma_free(NULL);
    CHECK_INT_EQ(used_blocks(), 0);
    CHECK_INT_EQ(aprisma_malloc_check(), 0);
}

TEST(malloc_reuses_and_merges_freed_blocks)
{
    char *a = aprisma_malloc(64);
    char *b = aprisma_malloc(64);
    char *c = aprisma_malloc(64);
    char *guard = aprisma_malloc(64);
    aprisma_free(b);
    CHECK(aprisma_malloc(64) == b);  // first fit takes the hole
    aprisma_free(a);
    aprisma_free(c);
    aprisma_free(b);                 // merges with both neighbours
    char *big = aprisma_malloc(3 * 64 + 2 * 32);
    CHECK(big == a);                 // the three blocks and two headers
    CHECK_INT_EQ(aprisma_malloc_check(), 0);
    aprisma_free(big);
    aprisma_free(guard);

    aprisma_malloc_stats_t stats;
    aprisma_malloc_stats(&stats);
    CHECK_INT_EQ(stats.used_blocks, 0);
    CHECK_INT_EQ(stats.free_blocks, 1);  // everything merged back
}

TEST(calloc_zeroes_reused_memory)
{
    unsigned char *dirty = aprisma_malloc(256);
    memset(dirty, 0xAB, 256);
    aprisma_free(dirty);
    unsigned char *clean = aprisma_calloc(64, 4);
    CHECK(clean == dirty);
    for (int i = 0; i < 256; i++)
        CHECK(clean[i] == 0);
    aprisma_free(clean);

    CHECK(aprisma_calloc(SIZE_MAX / 2, 4) == NULL);  // count * size overflows
    CHECK(aprisma_calloc((size_t)1 << 32, (size_t)1 << 32) == NULL);
}

TEST(realloc_keeps_the_contents)
{
    char *z = aprisma_realloc(NULL, 0);            // like malloc(0)
    CHECK(z != NULL);
    char *p = aprisma_realloc(NULL, 32);           // like malloc
    CHECK(p != NULL);
    for (int i = 0; i < 32; i++)
        p[i] = (char)i;

    char *blocker = aprisma_malloc(16);            // p cannot grow in place
    char *q = aprisma_realloc(p, 4000);
    CHECK(q != NULL && q != p);
    for (int i = 0; i < 32; i++)
        CHECK(q[i] == (char)i);

    char *r = aprisma_realloc(q, 16);              // shrinking stays in place
    CHECK(r == q && r[15] == 15);

    // The space freed by shrinking lets it grow in place again
    char *s = aprisma_realloc(r, 2000);
    CHECK(s == r && s[15] == 15);

    CHECK(aprisma_realloc(s, 0) == NULL);          // frees it
    aprisma_free(blocker);
    aprisma_free(z);
    CHECK_INT_EQ(aprisma_malloc_check(), 0);
}

TEST(large_blocks_grow_the_heap_and_are_given_back)
{
    size_t before = arena_brk;
    char *big = aprisma_malloc(4 << 20);
    CHECK(big != NULL);
    CHECK(arena_brk >= before + (3 << 20));  // a free tail at the end may be reused
    memset(big, 0x5A, 4 << 20);
    aprisma_free(big);
    CHECK(arena_brk < before + (256 << 10));  // trimmed back down
    CHECK_INT_EQ(aprisma_malloc_check(), 0);
}

TEST(out_of_memory_returns_null_and_keeps_the_heap)
{
    char *keep = aprisma_malloc(100);
    strcpy(keep, "still here");
    arena_limit = arena_brk + (1 << 20);

    CHECK(aprisma_malloc(2 << 20) == NULL);
    CHECK(aprisma_malloc((size_t)1 << 50) == NULL);
    CHECK(aprisma_realloc(keep, 2 << 20) == NULL);  // the old block stays valid
    CHECK_STR_EQ(keep, "still here");
    char *fits = aprisma_malloc(512 << 10);
    CHECK(fits != NULL);

    arena_limit = ARENA_SIZE;
    aprisma_free(fits);
    aprisma_free(keep);
    CHECK_INT_EQ(aprisma_malloc_check(), 0);
}

TEST(other_sbrk_users_leave_a_gap)
{
    char *a = aprisma_malloc(100);
    char *big = aprisma_malloc(512 << 10);  // takes the rest of the heap, so the next one must grow it
    char *foreign = aprisma_sbrk(3);        // the program moves the break itself, unaligned
    char *b = aprisma_malloc(256 << 10);
    CHECK(a && big && b && foreign != (void *)-1);
    CHECK((uintptr_t)b % 16 == 0);
    CHECK(b > foreign);                     // never hands out the program's bytes
    CHECK(b + (256 << 10) <= (char *)aprisma_sbrk(0));
    char *grown = aprisma_realloc(b, 512 << 10);  // the new region can still grow at the end
    CHECK(grown != NULL);
    b = grown;
    CHECK_INT_EQ(aprisma_malloc_check(), 0);
    aprisma_free(b);
    CHECK(arena_brk < (size_t)(foreign - (char *)arena) + (256 << 10));  // and is trimmed again
    aprisma_free(big);
    aprisma_free(a);
    CHECK_INT_EQ(used_blocks(), 0);
    CHECK_INT_EQ(aprisma_malloc_check(), 0);
}

TEST(double_free_and_bad_pointers_end_the_program)
{
    char *p = aprisma_malloc(40);
    volatile int stage = 0;
    message[0] = '\0';
    if (setjmp(exit_jump) == 0) {
        aprisma_free(p);
        stage = 1;
        aprisma_free(p);
        stage = 2;
    }
    CHECK_INT_EQ(stage, 1);
    CHECK_INT_EQ(exit_code, 127);
    CHECK(strstr(message, "double free") != NULL);

    char *q = aprisma_malloc(40);
    message[0] = '\0';
    stage = 0;
    if (setjmp(exit_jump) == 0) {
        aprisma_free(q + 16);  // aligned, but not the start of a block
        stage = 1;
    }
    CHECK_INT_EQ(stage, 0);
    CHECK(strstr(message, "invalid pointer") != NULL);
    aprisma_free(q);
    CHECK_INT_EQ(aprisma_malloc_check(), 0);
}

// Random malloc/free/realloc against shadow copies of what each block holds
TEST(random_operations_keep_every_block_intact)
{
    enum { SLOTS = 200, OPS = 30000 };
    static unsigned char *ptr[SLOTS];
    static size_t len[SLOTS];
    uint32_t seed = 12345;
    memset(ptr, 0, sizeof(ptr));

    for (int op = 0; op < OPS; op++) {
        seed = seed * 1103515245u + 12345u;
        int slot = (int)((seed >> 8) % SLOTS);
        unsigned char tag = (unsigned char)(slot * 7 + 1);
        size_t size = (seed >> 16) % 3000;
        if ((seed & 0xFF) == 0)
            size = 200000;  // now and then something large

        if (ptr[slot]) {
            for (size_t i = 0; i < len[slot]; i++)
                CHECK(ptr[slot][i] == (unsigned char)(tag + i));
        }
        if (ptr[slot] && (seed & 0x100)) {
            aprisma_free(ptr[slot]);
            ptr[slot] = NULL;
        } else {
            unsigned char *p = ptr[slot] ? aprisma_realloc(ptr[slot], size) : aprisma_malloc(size);
            CHECK(p != NULL || size == 0);
            size_t kept = len[slot] < size ? len[slot] : size;
            for (size_t i = 0; ptr[slot] && i < kept; i++)
                CHECK(p[i] == (unsigned char)(tag + i));
            for (size_t i = 0; i < size; i++)
                p[i] = (unsigned char)(tag + i);
            ptr[slot] = p;
            len[slot] = size;
        }
        if (op % 500 == 0)
            CHECK_INT_EQ(aprisma_malloc_check(), 0);
    }

    for (int slot = 0; slot < SLOTS; slot++)
        aprisma_free(ptr[slot]);
    CHECK_INT_EQ(used_blocks(), 0);
    CHECK_INT_EQ(aprisma_malloc_check(), 0);
    CHECK(arena_brk < (1 << 20));  // the free tail went back
}
