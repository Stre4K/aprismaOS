// test_misc.c
// qsort (src/libc/stdlib/qsort.c), <ctype.h> and the kernel bitmap helpers

#include "aprisma.h"
#include "test.h"

#include <stdlib.h>

static int compare_ints(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int is_sorted(const int *v, size_t n)
{
    for (size_t i = 1; i < n; i++)
        if (v[i - 1] > v[i])
            return 0;
    return 1;
}

TEST(qsort_small_array)
{
    int v[] = { 5, 3, 9, 1, 7 };
    aprisma_qsort(v, 5, sizeof(int), compare_ints);
    int expected[] = { 1, 3, 5, 7, 9 };
    CHECK_MEM_EQ(v, expected, sizeof(expected));
}

TEST(qsort_large_random_array)
{
    enum { N = 1000 };
    static int v[N];
    unsigned seed = 1;
    for (int i = 0; i < N; i++) {
        seed = seed * 1103515245 + 12345;
        v[i] = (int)(seed >> 8) % 500 - 250;  // includes duplicates and negatives
    }
    aprisma_qsort(v, N, sizeof(int), compare_ints);
    CHECK(is_sorted(v, N));
}

TEST(qsort_handles_empty_and_single)
{
    int one = 42;
    aprisma_qsort(NULL, 0, sizeof(int), compare_ints);
    aprisma_qsort(&one, 1, sizeof(int), compare_ints);
    CHECK_INT_EQ(one, 42);
}

TEST(ctype_digits_and_letters)
{
    CHECK(aprisma_isdigit('0') && aprisma_isdigit('9'));
    CHECK(!aprisma_isdigit('a') && !aprisma_isdigit('/'));
    CHECK(aprisma_isalpha('a') && aprisma_isalpha('Z'));
    CHECK(!aprisma_isalpha('1') && !aprisma_isalpha('['));
}

TEST(bitmap_set_test_clear)
{
    uint8_t map[4] = { 0 };

    aprisma_set_bit(map, 0);
    aprisma_set_bit(map, 9);
    aprisma_set_bit(map, 31);
    CHECK_INT_EQ(map[0], 0x01);
    CHECK_INT_EQ(map[1], 0x02);
    CHECK_INT_EQ(map[3], 0x80);

    CHECK(aprisma_test_bit(map, 9));
    CHECK(!aprisma_test_bit(map, 8));

    aprisma_clear_bit(map, 9);
    CHECK(!aprisma_test_bit(map, 9));
    CHECK_INT_EQ(map[1], 0x00);
    CHECK(aprisma_test_bit(map, 31));
}
