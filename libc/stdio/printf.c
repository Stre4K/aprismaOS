#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

// Function to convert integer to string
static void itoa(int value, char *str, int base) {
    char buffer[32];  // Temporary buffer
    int i = 0, is_negative = 0;

    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    if (value < 0 && base == 10) {
        is_negative = 1;
        value = -value;
    }

    while (value != 0) {
        int rem = value % base;
        buffer[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value /= base;
    }

    if (is_negative) buffer[i++] = '-';

    buffer[i] = '\0';

    // Reverse the string into str
    int j;
    for (j = 0; j < i; j++) {
        str[j] = buffer[i - j - 1];
    }
    str[j] = '\0';
}

// Function to convert float to string
static void ftoa(double value, char *str, int precision) {
    int int_part = (int)value;
    double frac_part = value - int_part;

    itoa(int_part, str, 10);
    size_t len = strlen(str);
    str[len] = '.';

    for (int i = 0; i < precision; i++) {
        frac_part *= 10;
    }

    itoa((int)(frac_part + 0.5), str + len + 1, 10);
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

        switch (*format) {
            case 'c': {  // Character
                char c = (char)va_arg(parameters, int);
                if (!print(&c, 1)) return -1;
                written++;
                break;
            }
            case 's': {  // String
                const char *str = va_arg(parameters, const char *);
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
                itoa(num, buffer, 10);
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
                itoa(num, buffer, 10);
                size_t len = strlen(buffer);
                if (maxrem < len) {
                    errno = EOVERFLOW;
                    return -1;
                }
                if (!print(buffer, len)) return -1;
                written += len;
                break;
            }
            case 'f': {  // Floating point
                double num = va_arg(parameters, double);
                char buffer[64];
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
            case 'x':
            case 'X': {  // Hexadecimal
                int num = va_arg(parameters, int);
                char buffer[32];
                itoa(num, buffer, 16);
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
