#include <kernel/shell/cmdline.h>
#include <kernel/drivers/serial.h>
#include <kernel/drivers/tty.h>
#include <kernel/printk.h>
#include <string.h>

#include <kernel/shell/shell.h>
#include <kernel/shell/commands.h>

#define CMDLINE_MAX_LEN 128

static char cmdline_buffer[CMDLINE_MAX_LEN];
static size_t cmdline_length = 0;
static size_t cmdline_cursor = 0;

void cmdline_init(void) {
    cmdline_length = 0;
    cmdline_cursor = 0;
    memset(cmdline_buffer, 0, CMDLINE_MAX_LEN);
    tty_set_cursor(0, tty_get_row());
}


void cmdline_input_char(char c) {
    terminal_scroll_to_bottom();

    if (c == '\n') {
        // The line is drawn straight into the terminal, not through printk,
        // so echo it to the serial log to show which command ran
        serial_write(cmdline_buffer);
        serial_putchar('\n');
        terminal_putchar('\n');
        cmdline_buffer[cmdline_length] = '\0';
        shell(cmdline_buffer);
        cmdline_init(); // reset for next command
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
    terminal_clear_line(row);
    for (size_t i = 0; i < cmdline_length; i++) {
        terminal_putentryat(cmdline_buffer[i], tty_get_color(), i, row);
    }
    terminal_render();
    tty_set_cursor(cmdline_cursor, row);
}

void cmdline_cursor_left(void) {
    if (cmdline_cursor > 0) {
        cmdline_cursor--;
        tty_set_cursor(cmdline_cursor, tty_get_row());
    }
}

void cmdline_cursor_right(void) {
    if (cmdline_cursor < cmdline_length) {
        cmdline_cursor++;
        tty_set_cursor(cmdline_cursor, tty_get_row());
    }
}


void cmdline_home(void) {
    cmdline_cursor = 0;
    tty_set_cursor(cmdline_cursor, tty_get_row());
}

void cmdline_end(void) {
    cmdline_cursor = cmdline_length;
    tty_set_cursor(cmdline_cursor, tty_get_row());
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
    tty_set_cursor(cmdline_length, tty_get_row());  // after the text, not over it
    printk("^C\n");
    cmdline_init();
}
