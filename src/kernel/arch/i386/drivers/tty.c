#include <kernel/drivers/tty.h>
#include <kernel/arch/portio.h>
#include <kernel/printk.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "vga.h"

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t *terminal_buffer;

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = VGA_MEMORY;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_setcolor(uint8_t color) { terminal_color = color; }

void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar(char c) {
    unsigned char uc = c;

    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
            // terminal_row = 0;
        }
    } else {
        terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT) {
                terminal_scroll();
            }
        }
    }
    vga_update_cursor(terminal_row, terminal_column); //
}

void terminal_write(const char *data, size_t size) {
    for (size_t i = 0; i < size; i++) terminal_putchar(data[i]);
}

void terminal_writestring(const char *data) {
    terminal_write(data, strlen(data));
}

void terminal_clear_line(const size_t terminal_row) {
    if (terminal_row >= VGA_HEIGHT) {
        return;
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_buffer[terminal_row * VGA_WIDTH + x] =
            vga_entry(' ', terminal_color);
    }
}

void terminal_scroll(void) {
    // Move each row up by copying the next row
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[(y - 1) * VGA_WIDTH + x] =
                terminal_buffer[y * VGA_WIDTH + x];
        }
    }

    // Clear the last row (fill it with spaces)
    terminal_clear_line(VGA_HEIGHT - 1);

    // Keep cursor on the last row
    terminal_row = VGA_HEIGHT - 1;
}


void vga_update_cursor(size_t row, size_t col) {
    uint16_t pos = row * VGA_WIDTH + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void tty_move_cursor(int dx, int dy) {
    int new_x = (int)terminal_column + dx;
    int new_y = (int)terminal_row + dy;

    if (new_x < 0) new_x = 0;
    if (new_x >= (int)VGA_WIDTH) new_x = VGA_WIDTH - 1;
    if (new_y < 0) new_y = 0;
    if (new_y >= (int)VGA_HEIGHT) new_y = VGA_HEIGHT - 1;

    terminal_column = (size_t)new_x;
    terminal_row = (size_t)new_y;

    vga_update_cursor(terminal_row, terminal_column);
}

void tty_set_cursor(size_t col, size_t row) {
    if (col >= VGA_WIDTH) col = VGA_WIDTH - 1;
    if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;

    terminal_column = col;
    terminal_row = row;

    vga_update_cursor(terminal_row, terminal_column);
}

void tty_backspace(void) {
    if (terminal_column > 0) {
        terminal_column--;
    } else if (terminal_row > 0) {
        terminal_row--;
        terminal_column = VGA_WIDTH - 1;
    } else {
        return; // Top-left — nothing to delete
    }

    terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
    vga_update_cursor(terminal_row, terminal_column);
}

size_t tty_get_row(void) {
    return terminal_row;
}

size_t tty_get_column(void) {
    return terminal_column;
}

uint8_t tty_get_color(void) {
    return terminal_color;
}

