#include <kernel/drivers/vga.h>
#include <kernel/drivers/tty.h>
#include <kernel/arch/portio.h>
#include <kernel/printk.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ================================================================
// ======================= TERMINAL STATE ========================
// ================================================================

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t *terminal_buffer;

/* Scrollback history */
#define TERM_HISTORY 1000
static uint16_t terminal_history[TERM_HISTORY][VGA_WIDTH];
static size_t history_head = 0;
static size_t history_view = 0;


// ================================================================
// ======================= CURSOR CONTROL ========================
// ================================================================

void vga_disable_cursor(void)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void vga_enable_cursor(void)
{
    /* cursor start */
    outb(0x3D4, 0x0A);
    outb(0x3D5, 14);

    /* cursor end */
    outb(0x3D4, 0x0B);
    outb(0x3D5, 15);
}

void vga_update_cursor(size_t row, size_t col) {
    uint16_t pos = row * VGA_WIDTH + col;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));

    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void tty_set_cursor(size_t col, size_t row) {
    if (col >= VGA_WIDTH) col = VGA_WIDTH - 1;
    if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;

    terminal_column = col;
    terminal_row = row;

    vga_update_cursor(terminal_row, terminal_column);
}

void tty_move_cursor(int dx, int dy)
{
    int new_x = (int)terminal_column + dx;
    int new_y = (int)terminal_row + dy;

    if (new_x < 0) new_x = 0;
    if (new_x >= VGA_WIDTH) new_x = VGA_WIDTH - 1;
    if (new_y < 0) new_y = 0;
    if (new_y >= VGA_HEIGHT) new_y = VGA_HEIGHT - 1;

    terminal_column = (size_t)new_x;
    terminal_row = (size_t)new_y;

    vga_update_cursor(terminal_row, terminal_column);
}



// ================================================================
// ======================= TERMINAL INITIALIZATION ===============
// ================================================================

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    terminal_buffer = VGA_MEMORY;

    memset(terminal_history, 0, sizeof(terminal_history));

    history_head = VGA_HEIGHT - 1;
    //history_head = 0;
    history_view = 0;

    terminal_render();
}

void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

// ================================================================
// ======================= RENDERING =============================
// ================================================================

// Render only the visible rows efficiently
static void terminal_render_range(size_t start_row, size_t end_row) {
    if (!terminal_buffer) return;

    for (size_t y = start_row; y < end_row && y < VGA_HEIGHT; y++) {
        size_t hist_row = history_view + y;
        if (hist_row >= TERM_HISTORY) break;

        // Use memcpy instead of nested loop
        memcpy(&terminal_buffer[y * VGA_WIDTH],
               &terminal_history[hist_row],
               VGA_WIDTH * sizeof(uint16_t));
    }
}

// Convenience: full screen
void terminal_render(void) {
    terminal_render_range(0, VGA_HEIGHT);
}

// ================================================================
// ======================= CHARACTER OUTPUT ======================
// ================================================================




void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y) {
    //size_t hist_row = history_head - terminal_row + y;
    size_t hist_row = history_view + y;

    if (hist_row >= TERM_HISTORY)
        return;

    if (x >= VGA_WIDTH) return;

    terminal_history[hist_row][x] = vga_entry(c, color);
}


void terminal_newline(void) {
    terminal_column = 0;
    terminal_row++;

    if (terminal_row >= VGA_HEIGHT) {
        terminal_row = VGA_HEIGHT - 1;

        history_head++;

        if (history_head >= TERM_HISTORY)
            history_head = TERM_HISTORY - 1;

        history_view = history_head - (VGA_HEIGHT - 1);

        memset(terminal_history[history_head], 0,
               VGA_WIDTH * sizeof(uint16_t));
    }

    // Always render after updating row
    terminal_render();
}


void terminal_putchar(char c) {

    // Always snap to newest output //
    history_view = history_head - (VGA_HEIGHT - 1);

    if (c == '\n') {
        terminal_newline();
    } else {
        terminal_putentryat(c, terminal_color,
                            terminal_column, terminal_row);

        terminal_column++;

        if (terminal_column >= VGA_WIDTH) {
            terminal_column = 0;
            terminal_newline();
        }
    }

    terminal_render();
 }




void terminal_write(const char *data, size_t size) {
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char *data) {
    terminal_write(data, strlen(data));
}


// ================================================================
// ======================= CLEAR / SCROLL ========================
// ================================================================


void terminal_clear_line(const size_t row) {
    size_t hist_row = history_view + row;

    if (hist_row >= TERM_HISTORY)
        return;

    for (size_t x = 0; x < VGA_WIDTH; x++)
        terminal_history[hist_row][x] =
            vga_entry(' ', terminal_color);
}

void terminal_clear_screen(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        terminal_clear_line(y);

    terminal_render();
}

void terminal_scroll_up(void) {
    if (history_view > 0) {
        history_view--;
        terminal_render();
        vga_disable_cursor();
    }
}

void terminal_scroll_down(void) {
    /*
    if (history_view < history_head - (VGA_HEIGHT - 1)) {
        history_view++;
        terminal_render();
    }
    */
    if (history_view + VGA_HEIGHT <= history_head) {
        history_view++;
        terminal_render();

        /*
        if (history_view == history_head - (VGA_HEIGHT - 1))
            vga_enable_cursor();
        */

        // show cursor only when we reach the bottom line
        if (history_view + VGA_HEIGHT == history_head + 1) {
            vga_enable_cursor();
            vga_update_cursor(terminal_row, terminal_column);
        }
    }
}

void terminal_scroll_to_bottom(void)
{
    history_view = history_head - (VGA_HEIGHT - 1);

    if (history_head >= VGA_HEIGHT - 1) {
        history_view = history_head - (VGA_HEIGHT - 1);
    } else {
        history_view = 0;
    }
    terminal_render();
    vga_enable_cursor();  // cursor visible again
    vga_update_cursor(terminal_row, terminal_column);
}


// ================================================================
// ======================= INTERACTIVE HELPERS ===================
// ================================================================

void tty_backspace(void)
{
    if (terminal_column > 0) {
        terminal_column--;
    } else if (terminal_row > 0) {
        terminal_row--;
        terminal_column = VGA_WIDTH - 1;
    } else {
        return;
    }

    terminal_putentryat(' ', terminal_color,
                        terminal_column, terminal_row);

    terminal_render();
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


