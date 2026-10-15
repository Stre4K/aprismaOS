// cmdline.c
// <kernel/shell/cmdline.h>
//
// Line editing for console input (the shell and programs reading with
// read()). Typed characters are echoed starting at the column where the
// line began, so input follows any prompt printed before it. Enter hands
// the finished line to the console's input queue.

#include <kernel/shell/cmdline.h>
#include <kernel/console.h>
#include <kernel/drivers/serial.h>
#include <kernel/drivers/tty.h>
#include <kernel/drivers/vga.h>
#include <kernel/printk.h>
#include <stdbool.h>
#include <string.h>

#define CMDLINE_MAX_LEN 128

static char cmdline_buffer[CMDLINE_MAX_LEN];
static size_t cmdline_length = 0;
static size_t cmdline_cursor = 0;
static size_t origin_col = 0;  // screen column of the line's first character

static void place_cursor(void) {
    tty_set_cursor(origin_col + cmdline_cursor, tty_get_row());
}

void cmdline_init(void) {
    cmdline_length = 0;
    cmdline_cursor = 0;
    memset(cmdline_buffer, 0, CMDLINE_MAX_LEN);
}

void cmdline_input_char(char c) {
    terminal_scroll_to_bottom();

    if (c == '\n') {
        // The line is drawn straight into the terminal, not through printk,
        // so echo it to the serial log too
        serial_write(cmdline_buffer);
        serial_putchar('\n');
        tty_set_cursor(origin_col + cmdline_length, tty_get_row());
        terminal_putchar('\n');

        console_line_entered(cmdline_buffer, cmdline_length);
        cmdline_init();
        return;
    }

    if (c == '\b') {
        if (cmdline_cursor > 0) {
            for (size_t i = cmdline_cursor - 1; i < cmdline_length - 1; i++) {
                cmdline_buffer[i] = cmdline_buffer[i + 1];
            }
            cmdline_length--;
            cmdline_cursor--;
            cmdline_buffer[cmdline_length] = 0;
            cmdline_redraw();
        }
        return;
    }

    // Only printable characters go into the line (no tabs, escapes, ...)
    if (c < ' ' || c > '~') {
        return;
    }

    // A new line starts where the output (e.g. a prompt) left the cursor
    if (cmdline_length == 0) {
        origin_col = tty_get_column();
    }

    if (cmdline_length < CMDLINE_MAX_LEN - 1) {
        for (size_t i = cmdline_length; i > cmdline_cursor; i--) {
            cmdline_buffer[i] = cmdline_buffer[i - 1];
        }
        cmdline_buffer[cmdline_cursor] = c;
        cmdline_length++;
        cmdline_cursor++;
        cmdline_redraw();
    }
}

void cmdline_redraw(void) {
    size_t row = tty_get_row();

    // Clear only the input part of the row, keeping a prompt before it
    for (size_t x = origin_col; x < VGA_WIDTH; x++) {
        terminal_putentryat(' ', tty_get_color(), x, row);
    }
    for (size_t i = 0; i < cmdline_length; i++) {
        terminal_putentryat(cmdline_buffer[i], tty_get_color(), origin_col + i, row);
    }
    terminal_render();
    place_cursor();
}

void cmdline_cursor_left(void) {
    if (cmdline_cursor > 0) {
        cmdline_cursor--;
        place_cursor();
    }
}

void cmdline_cursor_right(void) {
    if (cmdline_cursor < cmdline_length) {
        cmdline_cursor++;
        place_cursor();
    }
}

void cmdline_home(void) {
    cmdline_cursor = 0;
    place_cursor();
}

void cmdline_end(void) {
    cmdline_cursor = cmdline_length;
    place_cursor();
}

// Delete the character under the cursor
void cmdline_delete(void) {
    if (cmdline_cursor < cmdline_length) {
        cmdline_cursor++;
        cmdline_input_char('\b');
    }
}

// Ctrl+C: abandon the current line and start a new one
void cmdline_cancel(void) {
    terminal_scroll_to_bottom();
    tty_set_cursor(origin_col + cmdline_length, tty_get_row());  // after the text, not over it
    printk("^C\n");
    cmdline_init();
}

bool cmdline_is_empty(void) {
    return cmdline_length == 0;
}

// The screen was cleared: draw the line being typed at the top left
void cmdline_screen_cleared(void) {
    origin_col = 0;
    cmdline_redraw();
}
