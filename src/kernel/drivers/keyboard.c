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

// Each modifier key is tracked on its own: releasing one Shift must not
// cancel the other one that is still held
static bool left_shift, right_shift;
static bool left_ctrl, right_ctrl;
static bool extended_scancode;
static bool edit_mode;  // false = command-line mode, true = edit mode

static bool shift_held(void) { return left_shift || right_shift; }
static bool ctrl_held(void) { return left_ctrl || right_ctrl; }

/* ================================================================== */
/* Scancode decoding helpers */
/* ================================================================== */

// A character from a key press: into the command line, or onto the screen
// in edit mode
static void handle_char(char c) {
    if (edit_mode) {
        if (c == '\b') {
            tty_backspace();
        } else if (c == '\n' || (c >= ' ' && c <= '~')) {
            terminal_putchar(c);
        }
    } else {
        cmdline_input_char(c);
    }
}

static void handle_ctrl_key(uint8_t scancode) {
    switch (scancode) {
        case 0x2E: // Ctrl+C: abandon the current line
            if (edit_mode) {
                printk("^C\n");
            } else {
                cmdline_cancel();
            }
            break;
        case 0x26: // Ctrl+L: clear the screen, keep what is being typed
            terminal_clear_screen();
            if (!edit_mode) {
                cmdline_redraw();
            }
            break;
        case 0x12: // Ctrl+E: toggle edit mode
            edit_mode = !edit_mode;
            break;
        default:
            break;
    }
}

// Keys sent with the 0xE0 prefix (scancode set 1)
static void handle_extended_scancode(uint8_t sc) {
    uint8_t code = sc & 0x7F;
    bool release = sc & 0x80;

    if (code == 0x1D) {          // Right Ctrl
        right_ctrl = !release;
        return;
    }
    if (release) {
        return;
    }

    switch (code) {
        case 0x1C: // keypad Enter
            handle_char('\n');
            break;
        case 0x35: // keypad '/'
            handle_char('/');
            break;
        case 0x48: // UP
            if (edit_mode) tty_move_cursor(0, -1);
            break;
        case 0x50: // DOWN
            if (edit_mode) tty_move_cursor(0, 1);
            break;
        case 0x4B: // LEFT
            if (edit_mode) tty_move_cursor(-1, 0);
            else cmdline_cursor_left();
            break;
        case 0x4D: // RIGHT
            if (edit_mode) tty_move_cursor(1, 0);
            else cmdline_cursor_right();
            break;
        case 0x47: // HOME
            if (!edit_mode) cmdline_home();
            break;
        case 0x4F: // END
            if (!edit_mode) cmdline_end();
            break;
        case 0x53: // DELETE
            if (!edit_mode) cmdline_delete();
            break;
        case 0x49: // PGUP
            if (ctrl_held()) terminal_scroll_to_top();
            else terminal_scroll_up();
            break;
        case 0x51: // PGDN
            if (ctrl_held()) terminal_scroll_to_bottom();
            else terminal_scroll_down();
            break;
        default:
            // Everything else (Right Alt, Windows keys, Insert, media keys
            // and the "fake shift" codes 0x2A/0x36 some keyboards send
            // around navigation keys) is ignored
            break;
    }
}

static void handle_key_press(uint8_t scancode) {
    if (scancode >= sizeof(scancode_to_ascii)) {
        return;
    }
    char c = shift_held() ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];
    if (c) {
        handle_char(c);
    }
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
        extended_scancode = true;
        return;
    }

    // Handle extended scancode
    if (extended_scancode) {
        extended_scancode = false;
        handle_extended_scancode(sc);
        return;
    }

    switch (sc) {
        case LEFT_SHIFT_DOWN:  left_shift = true;   return;
        case LEFT_SHIFT_UP:    left_shift = false;  return;
        case RIGHT_SHIFT_DOWN: right_shift = true;  return;
        case RIGHT_SHIFT_UP:   right_shift = false; return;
        case LEFT_CTRL_DOWN:   left_ctrl = true;    return;
        case LEFT_CTRL_UP:     left_ctrl = false;   return;
        default: break;
    }

    if (sc & 0x80) {
        return;  // other key releases are ignored
    }

    if (ctrl_held()) {
        handle_ctrl_key(sc);
    } else {
        handle_key_press(sc);
    }
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
