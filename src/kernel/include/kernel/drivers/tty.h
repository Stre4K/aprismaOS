#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H

#include <stddef.h>
#include <stdint.h>

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y);
void terminal_write(const char *data, size_t size);
void terminal_writestring(const char *data);
void terminal_clear_line(const size_t terminal_row);
void terminal_scroll(void);

size_t tty_get_row(void);
size_t tty_get_column(void);
uint8_t tty_get_color(void);

void vga_update_cursor(size_t row, size_t col);
void tty_move_cursor(int dx, int dy);
void tty_set_cursor(size_t row, size_t col);
void tty_backspace(void);

#endif
