// printk.c
// <kernel/printk.h>
//
// Kernel console output. Formatting is done by libk's vformat(), the same
// engine behind printf/snprintf, so printk supports the full printf syntax
// (flags, width, precision, length modifiers) and shares its tests.

#include <kernel/drivers/tty.h>
#include <kernel/printk.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

static int terminal_sink(char c, void *ctx) {
    (void)ctx;
    terminal_putchar(c);
    return (unsigned char)c;
}

void printk(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    terminal_set_defer_render(true); // batch all output

    vformat(terminal_sink, NULL, fmt, args);

    terminal_set_defer_render(false);
    terminal_render(); // render all output at once
    va_end(args);
}
