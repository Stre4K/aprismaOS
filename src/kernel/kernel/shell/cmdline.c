#include <kernel/shell/cmdline.h>
#include <kernel/drivers/tty.h>
#include <kernel/printk.h>
#include <string.h>

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
    if (c == '\n') {
        terminal_putchar('\n');
        cmdline_buffer[cmdline_length] = '\0';
        printk("You typed: %s\n", cmdline_buffer);
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

