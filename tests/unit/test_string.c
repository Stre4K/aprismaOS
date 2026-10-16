// test_string.c
// <string.h>: src/libc/string/

#include "aprisma.h"
#include "test.h"

TEST(memcpy_copies_bytes)
{
    char dst[8] = "xxxxxxx";
    CHECK(aprisma_memcpy(dst, "abc", 3) == dst);
    CHECK_STR_EQ(dst, "abcxxxx");
}

TEST(memmove_overlap_forward)
{
    char buf[] = "abcdefgh";
    aprisma_memmove(buf + 2, buf, 5);  // destination after source
    CHECK_STR_EQ(buf, "ababcdeh");
}

TEST(memmove_overlap_backward)
{
    char buf[] = "abcdefgh";
    aprisma_memmove(buf, buf + 2, 5);  // destination before source
    CHECK_STR_EQ(buf, "cdefgfgh");
}

TEST(memset_uses_low_byte_of_value)
{
    unsigned char buf[4] = { 0 };
    CHECK(aprisma_memset(buf, 0x1AB, 3) == buf);
    CHECK_INT_EQ(buf[0], 0xAB);
    CHECK_INT_EQ(buf[2], 0xAB);
    CHECK_INT_EQ(buf[3], 0);
}

TEST(memcmp_compares_as_unsigned)
{
    CHECK_INT_EQ(aprisma_memcmp("abc", "abc", 3), 0);
    CHECK(aprisma_memcmp("abd", "abc", 3) > 0);
    CHECK(aprisma_memcmp("\x80", "\x01", 1) > 0);
    CHECK_INT_EQ(aprisma_memcmp("abX", "abY", 2), 0);  // only the first n bytes
}

TEST(strlen_counts_until_nul)
{
    CHECK_INT_EQ(aprisma_strlen(""), 0);
    CHECK_INT_EQ(aprisma_strlen("hello"), 5);
}

TEST(strcmp_orders_strings)
{
    CHECK_INT_EQ(aprisma_strcmp("abc", "abc"), 0);
    CHECK(aprisma_strcmp("abc", "abd") < 0);
    CHECK(aprisma_strcmp("abd", "abc") > 0);
    CHECK(aprisma_strcmp("ab", "abc") < 0);
    CHECK(aprisma_strcmp("\x80", "\x01") > 0);  // compares as unsigned char
}

TEST(strcpy_copies_terminator)
{
    char dst[8] = "xxxxxxx";
    CHECK(aprisma_strcpy(dst, "hi") == dst);
    CHECK_MEM_EQ(dst, "hi\0xxxx", 7);
}

TEST(strncpy_pads_with_zeros)
{
    char dst[6] = "xxxxx";
    aprisma_strncpy(dst, "ab", 5);
    CHECK_MEM_EQ(dst, "ab\0\0\0", 5);
}

TEST(strncpy_does_not_terminate_when_full)
{
    char dst[6] = "xxxxx";
    aprisma_strncpy(dst, "abcdef", 3);
    CHECK_MEM_EQ(dst, "abcxx", 5);
}

TEST(strrev_reverses_in_place)
{
    char s[] = "hello";
    aprisma_strrev(s);
    CHECK_STR_EQ(s, "olleh");

    char even[] = "ab";
    aprisma_strrev(even);
    CHECK_STR_EQ(even, "ba");

    char empty[] = "";
    aprisma_strrev(empty);
    CHECK_STR_EQ(empty, "");
}

TEST(strerror_names_errors)
{
    CHECK_STR_EQ(aprisma_strerror(1), "EPERM");
    CHECK_STR_EQ(aprisma_strerror(2), "ENOENT");
    CHECK_STR_EQ(aprisma_strerror(-12345), "Unknown error");
}

TEST(strncmp_compares_at_most_n)
{
    CHECK_INT_EQ(aprisma_strncmp("abcX", "abcY", 3), 0);
    CHECK(aprisma_strncmp("abcX", "abcY", 4) < 0);
    CHECK(aprisma_strncmp("ab", "abc", 5) < 0);   // stops at the shorter string
    CHECK_INT_EQ(aprisma_strncmp("same", "same", 100), 0);
    CHECK_INT_EQ(aprisma_strncmp("x", "y", 0), 0);
    CHECK(aprisma_strncmp("\x80", "\x01", 1) > 0);  // as unsigned char
}

TEST(strchr_and_strrchr_find_characters)
{
    const char *s = "/bin/hello";
    CHECK(aprisma_strchr(s, '/') == s);
    CHECK(aprisma_strrchr(s, '/') == s + 4);
    CHECK(aprisma_strchr(s, 'l') == s + 7);
    CHECK(aprisma_strrchr(s, 'l') == s + 8);
    CHECK(aprisma_strchr(s, 'z') == NULL);
    CHECK(aprisma_strrchr(s, 'z') == NULL);
    CHECK(aprisma_strchr(s, '\0') == s + 10);  // the terminator can be found
    CHECK(aprisma_strrchr(s, '\0') == s + 10);
}
