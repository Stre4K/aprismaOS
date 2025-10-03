// kernel/arch/i386/drivers/keyboard.c
#include <kernel/drivers/keyboard.h>
#include <kernel/arch/portio.h>
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
        default:
            printk("[UNKNOWN EXT: %02X]", code);
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
 * Keyboard interrupt handler
 * ====================================================================== */

static void keyboard_irq_handler(regs_t *regs) {
    (void)regs;
    uint8_t sc = inb(KBD_DATA_PORT);

    // Handle extended scancode prefix
    if (sc == 0xE0) {
        extended_scancode = 1;
        pic_send_eoi(KBD_IRQ);
        return;
    }

    // Handle extended scancode
    if (extended_scancode) {
        extended_scancode = 0;
        handle_extended_scancode(sc);
        pic_send_eoi(KBD_IRQ);
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

    if (sc & 0x80) {
        /* key release - ignore for now */
    }
        /* Tell PIC we're done */
    pic_send_eoi(KBD_IRQ); // IRQ1
}




/* ======================================================================
 * Keyboard driver Initialization
 * ====================================================================== */

/* Startup helper to register keyboard driver. Call from kernel init after idt/pic setup */
void keyboard_init(void) {
    /* mask/unmask IRQ1 as needed. We'll clear the mask for IRQ1 */
    pic_clear_mask_irq(KBD_IRQ);

    /* register handler for IRQ1 (IRQ number not vector): register for irq index 1 */
    register_irq_handler(KBD_IRQ, keyboard_irq_handler);
}

