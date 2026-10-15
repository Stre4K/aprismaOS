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


// Status polls before giving up on the controller. Each poll is an I/O port
// read (about 1 us on real hardware), so this waits roughly 100 ms.
#define PS2_TIMEOUT 100000

static bool ps2_wait_input(void) {
    for (int i = 0; i < PS2_TIMEOUT; i++) {
        if (!(inb(PS2_STATUS) & PS2_STATUS_INPUT_FULL))
            return true;
    }
    return false;
}

static bool ps2_wait_output(void) {
    for (int i = 0; i < PS2_TIMEOUT; i++) {
        if (inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)
            return true;
    }
    return false;
}

bool ps2_has_data(void) {
    return inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL;
}

bool ps2_write(uint8_t data) {
    if (!ps2_wait_input())
        return false;
    outb(PS2_DATA, data);
    return true;
}

int ps2_read(void) {
    if (!ps2_wait_output())
        return -1;
    return inb(PS2_DATA);
}

bool ps2_command(uint8_t cmd) {
    if (!ps2_wait_input())
        return false;
    outb(PS2_COMMAND, cmd);
    return true;
}

// Read and drop pending bytes; bounded in case the status port reads as
// all ones (no controller)
static void ps2_flush(void) {
    for (int i = 0; i < 16 && ps2_has_data(); i++)
        inb(PS2_DATA);
}

// Send a command that answers with one byte; -1 if the controller does not respond
static int ps2_command_read(uint8_t cmd) {
    if (!ps2_command(cmd))
        return -1;
    return ps2_read();
}

bool ps2_controller_init(void) {
    // Disable both ports; a controller that does not accept commands is absent
    if (!ps2_command(PS2_CMD_DISABLE_FIRST_PORT) || !ps2_command(PS2_CMD_DISABLE_SECOND_PORT)) {
        printk("[PS2] No PS/2 controller found\n");
        return false;
    }

    ps2_flush();

    // Controller self-test
    int ctrl_test = ps2_command_read(PS2_CMD_TEST_CONTROLLER);
    bool ctrl_ok = ctrl_test == PS2_RESP_SELF_TEST_PASSED;

    // Config: enable IRQ1 and translation, disable IRQ12
    int config = ps2_command_read(PS2_CMD_READ_CONFIG);
    if (config < 0) {
        printk("[PS2] Controller does not respond\n");
        return false;
    }
    config |= PS2_CFG_TRANSLATION | PS2_CFG_IRQ1;
    config &= ~PS2_CFG_IRQ2;
    if (!ps2_command(PS2_CMD_WRITE_CONFIG) || !ps2_write((uint8_t)config)) {
        printk("[PS2] Controller does not respond\n");
        return false;
    }

    // A dual-channel controller clears the second port's clock-disable bit
    // when that port is enabled; single-channel controllers have no port 2
    ps2_command(PS2_CMD_ENABLE_SECOND_PORT);
    int config2 = ps2_command_read(PS2_CMD_READ_CONFIG);
    bool dual_channel = config2 >= 0 && !(config2 & PS2_CFG_MOUSE_CLK);
    ps2_command(PS2_CMD_DISABLE_SECOND_PORT);

    // Interface tests (0x00 = passed)
    int port1_test = ps2_command_read(PS2_CMD_TEST_FIRST_PORT);
    bool port1_ok = port1_test == 0x00;
    int port2_test = dual_channel ? ps2_command_read(PS2_CMD_TEST_SECOND_PORT) : 0;
    bool port2_ok = port2_test == 0x00;

    // Enable the ports
    ps2_command(PS2_CMD_ENABLE_FIRST_PORT);
    if (dual_channel)
        ps2_command(PS2_CMD_ENABLE_SECOND_PORT);

    ps2_flush();

    // Print summary
    if (ctrl_ok && port1_ok && port2_ok) {
        printk("[PS2] Controller and ports initialized successfully%s.\n",
               dual_channel ? "" : " (single channel)");
    } else {
        printk("[PS2] Initialization FAILED:\n");
        if (!ctrl_ok)   printk("   - Controller self-test failed (0x%02X)\n", ctrl_test);
        if (!port1_ok)  printk("   - Keyboard port test failed (0x%02X)\n", port1_test);
        if (!port2_ok)  printk("   - Mouse port test failed (0x%02X)\n", port2_test);
    }

    return port1_ok;
}
