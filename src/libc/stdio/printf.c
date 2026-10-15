#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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
// Convert a non-negative double to fixed-point text with `precision`
// decimals (at most 18); returns the length. str needs room for up to 330
// characters. The caller handles the sign.
static size_t ftoa(double value, char *str, int precision) {
    size_t len = 0;

    if (value != value) {
        strcpy(str, "nan");
        return 3;
    }
    if (value > 1.7976931348623157e308) {
        strcpy(str, "inf");
        return 3;
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

// A parsed conversion: %[flags][width][.precision][length]conversion
typedef struct {
    bool left;        // '-': pad on the right
    bool zero;        // '0': pad numbers with zeros after the sign/prefix
    bool plus;        // '+': always print a sign
    bool space;       // ' ': print a space where a '+' would go
    bool alternate;   // '#': 0x/0X prefix for hex, leading 0 for octal
    int width;        // minimum field width, 0 if none
    int precision;    // -1 if none
    int length;       // number of 'l's, or -1/-2 for h/hh, 'z' maps to size_t
    bool size_t_arg;  // 'z'
} spec_t;

// Output state shared by the helpers below
typedef struct {
    int (*sink)(char c, void *ctx);
    void *ctx;
    int written;
    bool failed;
} out_t;

static void emit(out_t *out, const char *data, size_t len) {
    if (out->failed) return;
    if ((size_t)(INT_MAX - out->written) < len) {
        errno = EOVERFLOW;
        out->failed = true;
        return;
    }
    for (size_t i = 0; i < len; i++) {
        if (out->sink(data[i], out->ctx) == EOF) {
            out->failed = true;
            return;
        }
    }
    out->written += (int)len;
}

static void emit_repeat(out_t *out, char c, size_t count) {
    while (count-- > 0) emit(out, &c, 1);
}

// Print one field: prefix (sign, 0x), then `zeros` leading zeros, then the
// body, padded to the field width
static void emit_field(out_t *out, const spec_t *spec, const char *prefix,
                       size_t zeros, const char *body, size_t body_len, bool zero_pad_ok) {
    size_t prefix_len = strlen(prefix);
    size_t total = prefix_len + zeros + body_len;
    size_t pad = (spec->width > 0 && (size_t)spec->width > total) ? (size_t)spec->width - total : 0;
    bool zero_pad = spec->zero && !spec->left && zero_pad_ok;

    if (!spec->left && !zero_pad) emit_repeat(out, ' ', pad);
    emit(out, prefix, prefix_len);
    if (zero_pad) emit_repeat(out, '0', pad);
    emit_repeat(out, '0', zeros);
    emit(out, body, body_len);
    if (spec->left) emit_repeat(out, ' ', pad);
}

// Fetch an integer argument of the size given by the length modifier
static unsigned long long arg_unsigned(va_list *args, const spec_t *spec) {
    if (spec->size_t_arg) return va_arg(*args, size_t);
    switch (spec->length) {
        case -2: return (unsigned char)va_arg(*args, unsigned int);
        case -1: return (unsigned short)va_arg(*args, unsigned int);
        case 1: return va_arg(*args, unsigned long);
        case 2: return va_arg(*args, unsigned long long);
        default: return va_arg(*args, unsigned int);
    }
}

static long long arg_signed(va_list *args, const spec_t *spec) {
    if (spec->size_t_arg) return (long long)va_arg(*args, size_t);
    switch (spec->length) {
        case -2: return (signed char)va_arg(*args, int);
        case -1: return (short)va_arg(*args, int);
        case 1: return va_arg(*args, long);
        case 2: return va_arg(*args, long long);
        default: return va_arg(*args, int);
    }
}

// Print an integer conversion (d, i, u, o, x, X)
static void emit_integer(out_t *out, const spec_t *spec, char conversion, va_list *args) {
    char prefix[3] = "";
    unsigned long long magnitude;

    if (conversion == 'd' || conversion == 'i') {
        long long value = arg_signed(args, spec);
        // Negate as unsigned: the most negative value has no positive counterpart
        magnitude = value < 0 ? 0ULL - (unsigned long long)value : (unsigned long long)value;
        if (value < 0) strcpy(prefix, "-");
        else if (spec->plus) strcpy(prefix, "+");
        else if (spec->space) strcpy(prefix, " ");
    } else {
        magnitude = arg_unsigned(args, spec);
    }

    unsigned base = 10;
    if (conversion == 'o') base = 8;
    if (conversion == 'x' || conversion == 'X') base = 16;

    char digits[64];
    size_t len = utoa(magnitude, digits, base, conversion == 'X');

    // Precision 0 with value 0 prints no digits at all
    if (spec->precision == 0 && magnitude == 0) len = 0;

    if (spec->alternate && magnitude != 0) {
        if (conversion == 'x') strcpy(prefix, "0x");
        if (conversion == 'X') strcpy(prefix, "0X");
    }

    size_t zeros = (spec->precision > 0 && (size_t)spec->precision > len) ? (size_t)spec->precision - len : 0;
    if (spec->alternate && conversion == 'o' && zeros == 0 && !(len > 0 && digits[0] == '0'))
        zeros = 1;  // octal alternate form starts with a 0

    // A precision means "minimum digits", which replaces zero padding
    emit_field(out, spec, prefix, zeros, digits, len, spec->precision < 0);
}

// Parse flags, width, precision and length; *format points past the '%'
static void parse_spec(const char **format, spec_t *spec, va_list *args) {
    const char *f = *format;
    *spec = (spec_t){ .precision = -1 };

    for (;; f++) {
        if (*f == '-') spec->left = true;
        else if (*f == '0') spec->zero = true;
        else if (*f == '+') spec->plus = true;
        else if (*f == ' ') spec->space = true;
        else if (*f == '#') spec->alternate = true;
        else break;
    }

    if (*f == '*') {
        int width = va_arg(*args, int);
        if (width < 0) {  // a negative '*' width means left alignment
            spec->left = true;
            width = width == INT_MIN ? INT_MAX : -width;
        }
        spec->width = width;
        f++;
    } else {
        while (*f >= '0' && *f <= '9') {
            if (spec->width < INT_MAX / 10) spec->width = spec->width * 10 + (*f - '0');
            f++;
        }
    }

    if (*f == '.') {
        f++;
        spec->precision = 0;
        if (*f == '*') {
            int precision = va_arg(*args, int);
            spec->precision = precision < 0 ? -1 : precision;  // negative means none
            f++;
        } else {
            while (*f >= '0' && *f <= '9') {
                if (spec->precision < INT_MAX / 10) spec->precision = spec->precision * 10 + (*f - '0');
                f++;
            }
        }
    }

    if (*f == 'h') {
        spec->length = -1;
        if (*++f == 'h') {
            spec->length = -2;
            f++;
        }
    } else if (*f == 'l') {
        spec->length = 1;
        if (*++f == 'l') {
            spec->length = 2;
            f++;
        }
    } else if (*f == 'z') {
        spec->size_t_arg = true;
        f++;
    }

    *format = f;
}

// The formatting engine behind printf, vsnprintf and the kernel's printk:
// formats `fmt` and passes each character to `sink`, which returns EOF to
// stop with an error. Returns the number of characters, or -1 on error.
int vformat(int (*sink)(char c, void *ctx), void *ctx, const char *fmt, va_list args) {
    // Work on a copy: helpers take a va_list pointer, and the address of a
    // va_list parameter has the wrong type where va_list is an array (x86_64)
    va_list parameters;
    va_copy(parameters, args);

    // Plain cursor: parse_spec() advances it through a pointer
    const char *format = fmt;
    out_t out = { sink, ctx, 0, false };

    while (*format != '\0' && !out.failed) {
        if (*format != '%') {
            size_t amount = 1;
            while (format[amount] && format[amount] != '%') amount++;
            emit(&out, format, amount);
            format += amount;
            continue;
        }

        format++;  // Move past '%'

        spec_t spec;
        parse_spec(&format, &spec, &parameters);

        // A '%' at the end (possibly with flags): print the '%' and stop
        // instead of reading past the terminator
        if (*format == '\0') {
            emit(&out, "%", 1);
            break;
        }

        switch (*format) {
            case 'c': {  // Character
                char c = (char)va_arg(parameters, int);
                emit_field(&out, &spec, "", 0, &c, 1, false);
                break;
            }
            case 's': {  // String; precision limits the length
                const char *str = va_arg(parameters, const char *);
                if (str == NULL) str = "(null)";
                size_t len = 0;
                while (str[len] && (spec.precision < 0 || len < (size_t)spec.precision)) len++;
                emit_field(&out, &spec, "", 0, str, len, false);
                break;
            }
            case 'd':
            case 'i':
            case 'u':
            case 'o':
            case 'x':
            case 'X':
                emit_integer(&out, &spec, *format, &parameters);
                break;
#ifndef LIBK_NO_FLOAT
            case 'f': {  // Floating point, default precision 6
                double num = va_arg(parameters, double);
                const char *sign = "";
                if (num < 0) {
                    sign = "-";
                    num = -num;
                } else if (spec.plus) {
                    sign = "+";
                } else if (spec.space) {
                    sign = " ";
                }
                char buffer[330];
                size_t len = ftoa(num, buffer, spec.precision < 0 ? 6 : spec.precision);
                bool finite = buffer[0] >= '0' && buffer[0] <= '9';
                emit_field(&out, &spec, sign, 0, buffer, len, finite);
                break;
            }
#endif // LIBK_NO_FLOAT
            case 'p': {  // Pointer
                uintptr_t addr = (uintptr_t)va_arg(parameters, void *);
                if (addr == 0) {
                    emit_field(&out, &spec, "", 0, "(nil)", 5, false);
                } else {
                    char digits[32];
                    size_t len = utoa(addr, digits, 16, false);
                    emit_field(&out, &spec, "0x", 0, digits, len, false);
                }
                break;
            }
            case '%':  // Literal '%'
                emit(&out, "%", 1);
                break;
            default:  // Unsupported conversion: print it as-is
                emit(&out, "%", 1);
                emit(&out, format, 1);
                break;
        }

        format++;  // Move to next character
    }

    va_end(parameters);
    return out.failed ? -1 : out.written;
}

static int putchar_sink(char c, void *ctx) {
    (void)ctx;
    return putchar((unsigned char)c);
}

int vprintf(const char *restrict format, va_list args) {
    return vformat(putchar_sink, NULL, format, args);
}

int printf(const char *restrict format, ...) {
    va_list args;
    va_start(args, format);
    int written = vprintf(format, args);
    va_end(args);
    return written;
}
