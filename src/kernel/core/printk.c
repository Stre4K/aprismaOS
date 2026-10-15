#include <kernel/drivers/vga.h>
#include <kernel/drivers/tty.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>


// ----------------- Character and String Printing -----------------
static inline void print_char(char c) {
    terminal_putchar(c);
}

static inline void print_string(const char *s) {

    if (!s) {
        s = "(null)";
    }

    while (*s) {
        terminal_putchar(*s++);
    }
}


// ----------------- Integer Printing -----------------
static void print_uint64(uint64_t num, int base, bool uppercase) {
    char buf[32];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    if (num == 0) {
        print_char('0');
        return;
    }

    while (num > 0) {
        buf[i++] = digits[num % base];
        num /= base;
    }

    while (i > 0) print_char(buf[--i]);
}

static void print_uint32(uint32_t num, int base, bool uppercase) {
    print_uint64((uint64_t)num, base, uppercase);
}

static void print_int32(int32_t num) {
    if (num < 0) {
        print_char('-');
        num = -num;
    }
    print_uint32((uint32_t)num, 10, false);
}

// ----------------- Optimized printk -----------------
void printk(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    terminal_set_defer_render(true); // batch all output

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            bool long_long = false;

            // Skip zero-padding / width
            if (*fmt == '0') fmt++;
            while (*fmt >= '0' && *fmt <= '9') fmt++;

            // Handle 'l' and 'll'
            if (*fmt == 'l') {
                fmt++;
                if (*fmt == 'l') {
                    long_long = true;
                    fmt++;
                }
            }

            switch (*fmt) {
                case 'c': {
                    char c = (char)va_arg(args, int);
                    print_char(c);
                    break;
                }
                case 's': {
                    const char *s = va_arg(args, const char *);
                    print_string(s ? s : "(null)");
                    break;
                }
                case 'd':
                case 'i': {
                    int num = va_arg(args, int);
                    print_int32(num);
                    break;
                }
                case 'u': {
                    if (long_long) {
                        uint64_t num = va_arg(args, uint64_t);
                        print_uint64(num, 10, false);
                    } else {
                        uint32_t num = va_arg(args, uint32_t);
                        print_uint32(num, 10, false);
                    }
                    break;
                }
                case 'x':
                case 'X': {
                    bool uppercase = (*fmt == 'X');
                    if (long_long) {
                        uint64_t num = va_arg(args, uint64_t);
                        print_string("0x");
                        print_uint64(num, 16, uppercase);
                    } else {
                        uint32_t num = va_arg(args, uint32_t);
                        print_string("0x");
                        print_uint32(num, 16, uppercase);
                    }
                    break;
                }
                case 'p': {
                    uintptr_t ptr = (uintptr_t)va_arg(args, void *);
                    print_string("0x");
                    print_uint64((uint64_t)ptr, 16, false);
                    break;
                }
                case '%':
                    print_char('%');
                    break;
                default:
                    print_char('%');
                    print_char(*fmt);
                    break;
            }
        } else {
            print_char(*fmt);
        }
        fmt++;
    }

    terminal_set_defer_render(false);
    terminal_render(); // render all output at once
    va_end(args);
}
