// kernel/drivers/keyboard.c
#include <kernel/drivers/keyboard.h>
#include <kernel/drivers/ps2.h>
#include <kernel/arch/isr.h>
#include <kernel/arch/pic.h>
#include <kernel/printk.h>
#include <kernel/drivers/tty.h>
#include <kernel/shell/cmdline.h>
#include <stdint.h>
#include <stdbool.h>


/* ================================================================== */
/* Keyboard variables */
/* ================================================================== */

static int shift_pressed = 0;
static int extended_scancode = 0;
static int edit_mode = 0; // 0 = command-line mode, 1 = edit mode
static int ctrl_pressed = 0;

/* ================================================================== */
/* Keyboard IRQ handler helpers */
/* ================================================================== */

// Helper for extended scancodes
static void handle_extended_scancode(uint8_t sc) {
    uint8_t code = sc & 0x7F;
    int is_release = sc & 0x80;

    if (is_release)
        return;

    switch (code) {
        case 0x48:
            if (edit_mode) {
                tty_move_cursor(0, -1);
            }
            break;  // UP
        case 0x50:
            if (edit_mode) {
                tty_move_cursor(0, 1);
        }
        break;  // DOWN
        case 0x4B:
            if (!edit_mode) {
                cmdline_cursor_left();
                break;
            }
            tty_move_cursor(-1, 0);
            break; // LEFT
        case 0x4D:
            if (!edit_mode) {
                cmdline_cursor_right();
                break;
            }
            tty_move_cursor(1, 0);
            break;  // RIGHT
        case 0x49: // PGUP
            if (ctrl_pressed) {
                terminal_scroll_to_top();
            } else {
                terminal_scroll_up();
            }
            break;
        case 0x51: // PGDN
            if (ctrl_pressed) {
                terminal_scroll_to_bottom();
            } else {
                terminal_scroll_down();
            }
            break;
        default:
            printk("[UNKNOWN EXT: 0x%02X]", code);
            break;
    }
}


void handle_ctrl_key(uint8_t scancode) {
    switch (scancode) {
        case 0x2E: // Ctrl+C (scancode for 'c')
            // send SIGINT or break input
            printk("^C\n");
            break;
        case 0x26: // Ctrl+L (scancode for 'l')
            terminal_initialize(); // clear screen
            break;
        // add more as needed
        case 0x12: // Ctrl+E (scancode for 'e')
            edit_mode = !edit_mode;
            break;
        default:
            break;
    }
}

static void handle_key_press(uint8_t scancode, int shift_pressed) {
    char c = 0;

    if (scancode < sizeof(scancode_to_ascii)) {
        c = shift_pressed ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];
    }
    if (!c) {
        return;
    }
    if (edit_mode) {
        if (c == '\b') {
            tty_backspace();
            return;
        }
        terminal_putchar(c); // type at cursor
    } else {
        cmdline_input_char(c);  // simple input (append only)
    }

}


bool is_shift_press(uint8_t sc) {
    return sc == LEFT_SHIFT_DOWN || sc == RIGHT_SHIFT_DOWN;
}

bool is_shift_release(uint8_t sc) {
    return sc == LEFT_SHIFT_UP || sc == RIGHT_SHIFT_UP;
}

bool is_ctrl_press(uint8_t sc) {
    return sc == LEFT_CTRL_DOWN;
}

bool is_ctrl_release(uint8_t sc) {
    return sc == LEFT_CTRL_UP;
}

/* ======================================================================
 * Scancode buffer
 *
 * The interrupt handler only stores raw scancodes here. Decoding them and
 * running the shell happens in keyboard_process(), called from the kernel's
 * main loop with interrupts enabled. One producer (the IRQ handler, which
 * only writes kbd_head) and one consumer (keyboard_process, which only
 * writes kbd_tail) need no lock.
 * ====================================================================== */

#define KBD_BUFFER_SIZE 256  // power of two, so the indices can wrap freely

static volatile uint8_t kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_head;  // next slot to write
static volatile uint32_t kbd_tail;  // next slot to read

static void keyboard_irq_handler(regs_t *regs) {
    (void)regs;

    // A spurious or already handled interrupt has nothing to read, and
    // waiting for data here would stall every interrupt
    if (ps2_has_data()) {
        uint8_t sc = (uint8_t)ps2_read();
        uint32_t head = kbd_head;

        // Drop the scancode if the buffer is full
        if (head - kbd_tail < KBD_BUFFER_SIZE) {
            kbd_buffer[head % KBD_BUFFER_SIZE] = sc;
            kbd_head = head + 1;  // publish only after the data is stored
        }
    }

    pic_send_eoi(KBD_IRQ);
}

bool keyboard_has_input(void) {
    return kbd_head != kbd_tail;
}

/* ======================================================================
 * Scancode decoding (main loop context)
 * ====================================================================== */

static void keyboard_handle_scancode(uint8_t sc) {
    // Handle extended scancode prefix
    if (sc == 0xE0) {
        extended_scancode = 1;
        return;
    }

    // Handle extended scancode
    if (extended_scancode) {
        extended_scancode = 0;
        handle_extended_scancode(sc);
        return;
    }

    if (is_shift_press(sc)) {
        shift_pressed = 1;
    } else if (is_shift_release(sc)) {
        shift_pressed = 0;
    } else if (is_ctrl_press(sc)) {
        ctrl_pressed = 1;
    } else if (is_ctrl_release(sc)) {
        ctrl_pressed = 0;
    } else if (!(sc & 0x80)) { // key press
        if (ctrl_pressed) {
            handle_ctrl_key(sc);
        } else {
            handle_key_press(sc, shift_pressed);
        }
    }
    // other key releases are ignored
}

void keyboard_process(void) {
    while (kbd_tail != kbd_head) {
        uint8_t sc = kbd_buffer[kbd_tail % KBD_BUFFER_SIZE];
        kbd_tail++;
        keyboard_handle_scancode(sc);  // may run a whole shell command
    }
}


/* ======================================================================
 * Keyboard driver Initialization
 * ====================================================================== */

/* Startup helper to register keyboard driver. Call from kernel init after idt/pic setup and ps2 init */
void keyboard_init(void) {
    /* register handler for IRQ1 (IRQ number not vector): register for irq index 1 */
    register_irq_handler(KBD_IRQ, keyboard_irq_handler);

    // Enable scanning (keyboard command 0xF4)
    int ack = ps2_write(KBD_CMD_ENABLE_SCANNING) ? ps2_read() : -1;
    if (ack == PS2_RESP_ACK) {
        printk("[KEYBOARD] Initialized successfully\n");
    } else if (ack < 0) {
        printk("[KEYBOARD] No keyboard responded\n");
    } else {
        printk("[KEYBOARD] Init error: 0x%02X\n", ack);
    }

    /* mask/unmask IRQ1 as needed. We'll clear the mask for IRQ1 */
    pic_clear_mask_irq(KBD_IRQ);
}
