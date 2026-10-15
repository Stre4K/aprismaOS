// ps2.c
// PS/2 controller initialization
// <kernel/driver/ps2.h>
// Author: Stre4k
// Date: 2026-03-18

#include <kernel/drivers/ps2.h>
#include <kernel/arch/portio.h>
#include <kernel/printk.h>
#include <stdint.h>
#include <stdbool.h>


static void ps2_wait_input(void) {
    while (inb(PS2_STATUS) & PS2_STATUS_INPUT_FULL);
}

static void ps2_wait_output(void) {
    while (!(inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL));
}

void ps2_write(uint8_t data) {
    ps2_wait_input();
    outb(PS2_DATA, data);
}

uint8_t ps2_read(void) {
    ps2_wait_output();
    return inb(PS2_DATA);
}

void ps2_command(uint8_t cmd) {
    ps2_wait_input();
    outb(PS2_COMMAND, cmd);
}

void ps2_controller_init(void) {
    // Disable both ports
    ps2_command(PS2_CMD_DISABLE_FIRST_PORT);
    ps2_command(PS2_CMD_DISABLE_SECOND_PORT);

    // Flush output buffer
    while (inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) {
        ps2_read();
    }

    bool ctrl_ok = true;
    bool port1_ok = true;
    bool port2_ok = true;

    // Controller self-test
    ps2_command(PS2_CMD_TEST_CONTROLLER);
    ps2_wait_output();
    uint8_t ctrl_test = ps2_read();
    if (ctrl_test != PS2_RESP_SELF_TEST_PASSED) {
        ctrl_ok = false;
    }

    // Read config byte
    ps2_command(PS2_CMD_READ_CONFIG);
    ps2_wait_output();
    uint8_t config = ps2_read();

    // Modify config: enable IRQ1, translation; disable IRQ2
    config |= PS2_CFG_TRANSLATION | PS2_CFG_IRQ1;
    config &= ~PS2_CFG_IRQ2;

    // Write back
    ps2_command(PS2_CMD_WRITE_CONFIG);
    ps2_write(config);

    // Enable and test first port (keyboard)
    ps2_command(PS2_CMD_ENABLE_FIRST_PORT);
    ps2_command(PS2_CMD_TEST_FIRST_PORT);
    ps2_wait_output();
    uint8_t port1_test = ps2_read();
    if (port1_test != 0x00) {
        port1_ok = false;
    }

    // Enable and test second port (mouse)
    ps2_command(PS2_CMD_ENABLE_SECOND_PORT);
    ps2_command(PS2_CMD_TEST_SECOND_PORT);
    ps2_wait_output();
    uint8_t port2_test = ps2_read();
    if (port2_test != 0x00) {
        port2_ok = false;
    }

    // Enable both ports
    ps2_command(PS2_CMD_ENABLE_FIRST_PORT);
    ps2_command(PS2_CMD_ENABLE_SECOND_PORT);

    // Flush output buffer
    while (inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) {
        ps2_read();
    }

    // Print summary
    if (ctrl_ok && port1_ok && port2_ok) {
        printk("[PS2] Controller and ports initialized successfully.\n");
    } else {
        printk("[PS2] Initialization FAILED:\n");
        if (!ctrl_ok)   printk("   - Controller self-test failed (0x%02X)\n", ctrl_test);
        if (!port1_ok)  printk("   - Keyboard port test failed (0x%02X)\n", port1_test);
        if (!port2_ok)  printk("   - Mouse port test failed (0x%02X)\n", port2_test);
    }
}
