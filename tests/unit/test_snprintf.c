// test_snprintf.c
// snprintf(): src/libc/stdio/snprintf.c (on top of the shared vformat engine)

#include "aprisma.h"
#include "test.h"

TEST(snprintf_formats_into_buffer)
{
    char buf[32];
    CHECK_INT_EQ(aprisma_snprintf(buf, sizeof(buf), "%s=%05d", "id", 42), 8);
    CHECK_STR_EQ(buf, "id=00042");
}

TEST(snprintf_truncates_and_returns_full_length)
{
    char buf[6] = "xxxxx";
    CHECK_INT_EQ(aprisma_snprintf(buf, sizeof(buf), "hello world"), 11);
    CHECK_STR_EQ(buf, "hello");  // size - 1 characters plus the terminator
}

TEST(snprintf_size_zero_writes_nothing)
{
    char buf[4] = "abc";
    CHECK_INT_EQ(aprisma_snprintf(buf, 0, "%d", 12345), 5);
    CHECK_STR_EQ(buf, "abc");
}

TEST(snprintf_size_one_gives_empty_string)
{
    char buf[4] = "abc";
    CHECK_INT_EQ(aprisma_snprintf(buf, 1, "%d", 7), 1);
    CHECK_STR_EQ(buf, "");
}

TEST(snprintf_exact_fit)
{
    char buf[4];
    CHECK_INT_EQ(aprisma_snprintf(buf, sizeof(buf), "abc"), 3);
    CHECK_STR_EQ(buf, "abc");
}
