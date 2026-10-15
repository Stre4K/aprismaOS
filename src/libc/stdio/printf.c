#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Function to print a string to output
static bool print(const char *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t i = 0; i < length; i++) {
        if (putchar(bytes[i]) == EOF) {
            return false;
        }
    }
    return true;
}

// Convert an unsigned value to digits in the given base; returns the length.
// Signed values are printed as a '-' followed by their magnitude, so every
// integer conversion goes through here and INT_MIN needs no special case.
static size_t utoa(unsigned long long value, char *str, unsigned base, bool uppercase) {
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char buffer[64];
    size_t i = 0;

    do {
        buffer[i++] = digits[value % base];
        value /= base;
    } while (value != 0);

    for (size_t j = 0; j < i; j++) {
        str[j] = buffer[i - j - 1];
    }
    str[i] = '\0';
    return i;
}

#ifndef LIBK_NO_FLOAT
// Convert a double to fixed-point text with `precision` decimals (at most
// 18); returns the length. str needs room for up to 330 characters.
static size_t ftoa(double value, char *str, int precision) {
    size_t len = 0;

    if (value != value) {
        strcpy(str, "nan");
        return 3;
    }
    if (value < 0) {
        str[len++] = '-';
        value = -value;
    }
    if (value > 1.7976931348623157e308) {
        strcpy(str + len, "inf");
        return len + 3;
    }

    if (precision < 0) precision = 0;
    if (precision > 18) precision = 18;

    // Values too big for an unsigned long long are scaled down and printed
    // with trailing zeros; the digits beyond double precision are not exact
    int extra_zeros = 0;
    while (value >= 18446744073709551616.0) {
        value /= 10;
        extra_zeros++;
    }

    unsigned long long scale = 1;
    for (int i = 0; i < precision; i++) scale *= 10;

    unsigned long long int_part = (unsigned long long)value;
    unsigned long long frac = (unsigned long long)((value - (double)int_part) * (double)scale + 0.5);
    if (frac >= scale) {  // rounding carried into the integer part
        int_part++;
        frac -= scale;
    }

    len += utoa(int_part, str + len, 10, false);
    while (extra_zeros-- > 0) str[len++] = '0';

    if (precision > 0) {
        char digits[24];
        size_t n = utoa(frac, digits, 10, false);
        str[len++] = '.';
        for (size_t i = n; i < (size_t)precision; i++) str[len++] = '0';  // leading zeros
        memcpy(str + len, digits, n);
        len += n;
    }

    str[len] = '\0';
    return len;
}
#endif // LIBK_NO_FLOAT

// Function to convert pointer to hex string
static void ptoa(void *ptr, char *str) {
    uintptr_t addr = (uintptr_t)ptr;
    char buffer[32];
    int i = 0;

    if (addr == 0) {
        strcpy(str, "(nil)");
        return;
    }

    while (addr != 0) {
        int rem = addr % 16;
        buffer[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        addr /= 16;
    }

    str[0] = '0';
    str[1] = 'x';
    int j;
    for (j = 0; j < i; j++) {
        str[j + 2] = buffer[i - j - 1];
    }
    str[j + 2] = '\0';
}

// Custom printf function
int printf(const char *restrict format, ...) {
    va_list parameters;
    va_start(parameters, format);

    int written = 0;

    while (*format != '\0') {
        size_t maxrem = INT_MAX - written;

        if (*format != '%') {
            size_t amount = 1;
            while (format[amount] && format[amount] != '%') amount++;

            if (maxrem < amount) {
                errno = EOVERFLOW;
                return -1;
            }
            if (!print(format, amount)) return -1;
            format += amount;
            written += amount;
            continue;
        }

        format++;  // Move past '%'

        // A lone '%' at the end: print it and stop instead of reading past
        // the terminator
        if (*format == '\0') {
            if (maxrem < 1) {
                errno = EOVERFLOW;
                return -1;
            }
            if (!print("%", 1)) return -1;
            written++;
            break;
        }

        switch (*format) {
            case 'c': {  // Character
                char c = (char)va_arg(parameters, int);
                if (!print(&c, 1)) return -1;
                written++;
                break;
            }
            case 's': {  // String
                const char *str = va_arg(parameters, const char *);
                if (str == NULL) str = "(null)";
                size_t len = strlen(str);
                if (maxrem < len) {
                    errno = EOVERFLOW;
                    return -1;
                }
                if (!print(str, len)) return -1;
                written += len;
                break;
            }
            case 'd':
            case 'i': {  // Signed integer
                int num = va_arg(parameters, int);
                char buffer[32];
                // Negate as unsigned: -INT_MIN does not fit in an int
                unsigned int magnitude = num < 0 ? 0u - (unsigned int)num : (unsigned int)num;
                size_t sign = 0;
                if (num < 0) buffer[sign++] = '-';
                utoa(magnitude, buffer + sign, 10, false);
                size_t len = strlen(buffer);
                if (maxrem < len) {
                    errno = EOVERFLOW;
                    return -1;
                }
                if (!print(buffer, len)) return -1;
                written += len;
                break;
            }
            case 'u': {  // Unsigned integer
                unsigned int num = va_arg(parameters, unsigned int);
                char buffer[32];
                utoa(num, buffer, 10, false);
                size_t len = strlen(buffer);
                if (maxrem < len) {
                    errno = EOVERFLOW;
                    return -1;
                }
                if (!print(buffer, len)) return -1;
                written += len;
                break;
            }
#ifndef LIBK_NO_FLOAT
            case 'f': {  // Floating point
                double num = va_arg(parameters, double);
                char buffer[330];
                ftoa(num, buffer, 6);  // Default precision: 6 decimal places
                size_t len = strlen(buffer);
                if (maxrem < len) {
                    errno = EOVERFLOW;
                    return -1;
                }
                if (!print(buffer, len)) return -1;
                written += len;
                break;
            }
#endif // LIBK_NO_FLOAT
            case 'x':
            case 'X': {  // Hexadecimal
                unsigned int num = va_arg(parameters, unsigned int);
                char buffer[32];
                utoa(num, buffer, 16, *format == 'X');
                size_t len = strlen(buffer);
                if (maxrem < len) {
                    errno = EOVERFLOW;
                    return -1;
                }
                if (!print(buffer, len)) return -1;
                written += len;
                break;
            }
            case 'p': {
                void *ptr = va_arg(parameters, void *);
                char buffer[32];
                ptoa(ptr, buffer);
                size_t len = strlen(buffer);
                if (maxrem < len) {
                    errno = EOVERFLOW;
                    return -1;
                }
                if (!print(buffer, len)) return -1;
                written += len;
                break;
            }
            case '%': {  // Literal '%'
                if (!print("%", 1)) return -1;
                written++;
                break;
            }
            default: {  // Unsupported format specifier
                if (!print("%", 1)) return -1;
                if (!print(format, 1)) return -1;
                written += 2;
                break;
            }
        }

        format++;  // Move to next character
    }

    va_end(parameters);
    return written;
}
