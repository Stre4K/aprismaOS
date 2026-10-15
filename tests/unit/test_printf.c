// test_printf.c
// printf(): src/libc/stdio/printf.c

#include "aprisma.h"
#include "test.h"

#include <limits.h>
#include <math.h>

// Run printf and check the exact bytes it printed and its return value
#define CHECK_PRINTF(expected, ...)                                           \
    do {                                                                      \
        capture_reset();                                                      \
        int ret_ = aprisma_printf(__VA_ARGS__);                               \
        CHECK_STR_EQ(capture_text(), expected);                               \
        CHECK_INT_EQ(capture_length(), strlen(expected));                     \
        CHECK_INT_EQ(ret_, (int)strlen(expected));                            \
    } while (0)

TEST(printf_plain_text)
{
    CHECK_PRINTF("hello, world", "hello, world");
    CHECK_PRINTF("", "");
}

TEST(printf_percent_sign)
{
    CHECK_PRINTF("100%", "100%%");
}

TEST(printf_char_and_string)
{
    CHECK_PRINTF("a=x s=abc", "a=%c s=%s", 'x', "abc");
}

TEST(printf_signed_decimal)
{
    CHECK_PRINTF("0 42 -42", "%d %d %i", 0, 42, -42);
    CHECK_PRINTF("2147483647", "%d", INT_MAX);
}

TEST(printf_int_min)
{
    CHECK_PRINTF("-2147483648", "%d", INT_MIN);
}

TEST(printf_unsigned_above_int_max)
{
    CHECK_PRINTF("3000000000", "%u", 3000000000u);
    CHECK_PRINTF("4294967295", "%u", UINT_MAX);
}

TEST(printf_hex_lowercase)
{
    CHECK_PRINTF("ff 0 deadbeef", "%x %x %x", 255, 0, 0xdeadbeefu);
}

TEST(printf_hex_uppercase)
{
    CHECK_PRINTF("FF", "%X", 255);
}

TEST(printf_hex_of_negative_int)
{
    CHECK_PRINTF("ffffffff", "%x", -1);
}

TEST(printf_pointer)
{
    CHECK_PRINTF("0x1234", "%p", (void *)0x1234);
    CHECK_PRINTF("(nil)", "%p", (void *)0);
}

TEST(printf_null_string)
{
    CHECK_PRINTF("(null)", "%s", (char *)NULL);
}

TEST(printf_unknown_specifier_is_printed)
{
    CHECK_PRINTF("%q", "%q");
}

TEST(printf_trailing_percent)
{
    // The format must not be read past its terminator. The buffer after the
    // NUL holds text that must never show up in the output.
    static const char format[] = "a%\0SHOULD NOT PRINT";
    capture_reset();
    int ret = aprisma_printf(format);
    CHECK_INT_EQ(capture_length(), 2);
    CHECK_MEM_EQ(capture_text(), "a%", 2);
    CHECK_INT_EQ(ret, 2);
}

TEST(printf_float)
{
    CHECK_PRINTF("1.500000", "%f", 1.5);
    CHECK_PRINTF("2.250000", "%f", 2.25);
}

TEST(printf_float_negative_fraction)
{
    CHECK_PRINTF("-0.500000", "%f", -0.5);
    CHECK_PRINTF("-1.500000", "%f", -1.5);
}

TEST(printf_float_leading_zero_in_fraction)
{
    CHECK_PRINTF("1.050000", "%f", 1.05);
}

TEST(printf_float_rounding_carries)
{
    CHECK_PRINTF("1.000000", "%f", 0.9999999);
    CHECK_PRINTF("0.000001", "%f", 0.0000005);
}

TEST(printf_float_beyond_int_range)
{
    CHECK_PRINTF("3000000000.000000", "%f", 3e9);
    CHECK_PRINTF("-3000000000.500000", "%f", -3000000000.5);
}

TEST(printf_float_special_values)
{
    CHECK_PRINTF("inf -inf nan", "%f %f %f", HUGE_VAL, -HUGE_VAL, NAN);
}

TEST_KNOWN_BUG(printf_width_and_zero_padding,
               "width and flags are not parsed, so later arguments shift")
{
    // Unsupported flags must at least not shift the remaining arguments
    CHECK_PRINTF("000000ff|7", "%08x|%d", 255, 7);
}

TEST(puts_appends_newline)
{
    capture_reset();
    aprisma_puts("line");
    CHECK_STR_EQ(capture_text(), "line\n");
}
