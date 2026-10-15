// test_printf.c
// printf(): src/libc/stdio/printf.c

#include "aprisma.h"
#include "test.h"

#include <limits.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>

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

TEST(printf_width_and_zero_padding)
{
    // Unsupported flags must at least not shift the remaining arguments
    CHECK_PRINTF("000000ff|7", "%08x|%d", 255, 7);
}

TEST(printf_width_aligns_right_and_left)
{
    CHECK_PRINTF("[   42][42   ]", "[%5d][%-5d]", 42, 42);
    CHECK_PRINTF("[    ab][ab    ]", "[%6s][%-6s]", "ab", "ab");
    CHECK_PRINTF("[  x]", "[%3c]", 'x');
    CHECK_PRINTF("[toolong]", "[%3s]", "toolong");  // width is a minimum
}

TEST(printf_zero_padding_goes_after_the_sign)
{
    CHECK_PRINTF("-0042", "%05d", -42);
    CHECK_PRINTF("-42  ", "%-05d", -42);  // '-' overrides '0'
}

TEST(printf_plus_and_space_flags)
{
    CHECK_PRINTF("+5 -5  5", "%+d %+d % d", 5, -5, 5);
}

TEST(printf_alternate_form)
{
    CHECK_PRINTF("0xff 0XFF 0 010", "%#x %#X %#x %#o", 255, 255, 0, 8);
}

TEST(printf_octal)
{
    CHECK_PRINTF("10 377", "%o %o", 8, 255);
}

TEST(printf_star_width)
{
    CHECK_PRINTF("   7|7   |7   ", "%*d|%-*d|%*d", 4, 7, 4, 7, -4, 7);
}

TEST(printf_precision)
{
    CHECK_PRINTF("3.14 3 2.718282", "%.2f %.0f %f", 3.14159, 2.7, 2.718281828);
    CHECK_PRINTF("abc", "%.3s", "abcdef");
    CHECK_PRINTF("00042 -00042", "%.5d %.5d", 42, -42);
    CHECK_PRINTF("   00042", "%08.5d", 42);  // precision disables zero padding
    CHECK_PRINTF("", "%.0d", 0);             // zero with precision 0 has no digits
}

TEST(printf_float_width_and_sign)
{
    CHECK_PRINTF("  1.50|-01.50|+1.500000", "%6.2f|%06.2f|%+f", 1.5, -1.5, 1.5);
    CHECK_PRINTF("   inf", "%06f", HUGE_VAL);  // no zero padding for inf/nan
}

TEST(printf_length_modifiers)
{
    CHECK_PRINTF("-5 -9000000000", "%ld %lld", -5L, -9000000000LL);
    CHECK_PRINTF("18446744073709551615 123456789abcdef", "%llu %llx",
                 18446744073709551615ULL, 0x123456789abcdefULL);
    CHECK_PRINTF("42 44 4464", "%zu %hhd %hd", (size_t)42, 300, 70000);
}

TEST(printf_pointer_with_width)
{
    CHECK_PRINTF("[  0x1f]", "[%6p]", (void *)0x1f);
}

// Compare against the host C library on random integer conversions with
// random flags, width, precision and length (deterministic seed)
TEST(printf_integers_match_host_libc)
{
    static const char *const conversions = "diuoxX";
    static const char *const lengths[] = { "", "hh", "h", "l", "ll", "z" };
    unsigned long long seed = 0x9E3779B97F4A7C15ULL;

    for (int i = 0; i < 5000; i++) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        unsigned long long r = seed;

        char conversion = conversions[r % 6];
        const char *length = lengths[(r >> 3) % 6];
        bool is_signed = conversion == 'd' || conversion == 'i';

        char format[32];
        size_t n = 0;
        format[n++] = '%';
        if (r & (1ULL << 6)) format[n++] = '-';
        if (r & (1ULL << 7)) format[n++] = '0';
        if (r & (1ULL << 8)) format[n++] = '+';
        if (r & (1ULL << 9)) format[n++] = ' ';
        if ((r & (1ULL << 10)) && !is_signed && conversion != 'u') format[n++] = '#';
        if (r & (1ULL << 11)) n += sprintf(format + n, "%d", (int)((r >> 12) % 13));
        if (r & (1ULL << 16)) n += sprintf(format + n, ".%d", (int)((r >> 17) % 11));
        n += sprintf(format + n, "%s%c", length, conversion);

        // Mix small, boundary and random values
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        unsigned long long value = seed;
        switch ((r >> 21) % 4) {
            case 0: value %= 1000; break;
            case 1: value = (r >> 23) % 2 ? 0 : (unsigned long long)-1; break;
            case 2: value = (unsigned long long)(long long)(int)value; break;
            default: break;
        }

        char expected[128];
        capture_reset();
        if (!strcmp(length, "l") || !strcmp(length, "z")) {
            snprintf(expected, sizeof(expected), format, (long)value);
            aprisma_printf(format, (long)value);
        } else if (!strcmp(length, "ll")) {
            snprintf(expected, sizeof(expected), format, (long long)value);
            aprisma_printf(format, (long long)value);
        } else {
            snprintf(expected, sizeof(expected), format, (int)value);
            aprisma_printf(format, (int)value);
        }

        if (strcmp(capture_text(), expected) != 0) {
            test_fail(__FILE__, __LINE__, "format \"%s\" value %llu: host printed \"%s\", got \"%s\"",
                      format, value, expected, capture_text());
            return;
        }
    }
}

TEST(puts_appends_newline)
{
    capture_reset();
    aprisma_puts("line");
    CHECK_STR_EQ(capture_text(), "line\n");
}
