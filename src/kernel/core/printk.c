// printk.c
// <kernel/printk.h>
//
// Kernel console output, shown on the screen and mirrored to the serial
// port. Formatting is done by libk's vformat(), the same engine behind
// printf/snprintf, so printk supports the full printf syntax (flags, width,
// precision, length modifiers) and shares its tests.

#include <kernel/drivers/serial.h>
#include <kernel/drivers/tty.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

static int console_sink(char c, void *ctx) {
    (void)ctx;
    terminal_putchar(c);
    serial_putchar(c);
    return (unsigned char)c;
}

void printk(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // One printk is never interleaved with another thread's output
    preempt_disable();
    terminal_set_defer_render(true); // batch all output

    vformat(console_sink, NULL, fmt, args);

    terminal_set_defer_render(false);
    terminal_render(); // render all output at once
    preempt_enable();
    va_end(args);
}
