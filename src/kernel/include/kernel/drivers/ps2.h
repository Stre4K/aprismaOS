#ifndef __KERNEL_DRIVERS_PS2_H__
#define __KERNEL_DRIVERS_PS2_H__

#include <stdbool.h>
#include <stdint.h>

/* PS/2 ports */
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_COMMAND 0x64

/* Status register bits */
#define PS2_STATUS_OUTPUT_FULL (1 << 0)
#define PS2_STATUS_INPUT_FULL  (1 << 1)

/* Controller config byte bits */
#define PS2_CFG_IRQ1        (1 << 0)
#define PS2_CFG_IRQ2        (1 << 1)
#define PS2_CFG_ZERO1       (1 << 3) // Must be zero
#define PS2_CFG_ZERO2       (1 << 4) // Must be zero
#define PS2_CFG_SYSTEM_FLAG (1 << 2)
#define PS2_CFG_MOUSE_CLK   (1 << 5)
#define PS2_CFG_TRANSLATION (1 << 6)

// PS/2 Controller Commands
#define PS2_CMD_DISABLE_FIRST_PORT   0xAD
#define PS2_CMD_DISABLE_SECOND_PORT  0xA7
#define PS2_CMD_ENABLE_FIRST_PORT    0xAE
#define PS2_CMD_ENABLE_SECOND_PORT   0xA8
#define PS2_CMD_READ_CONFIG          0x20
#define PS2_CMD_WRITE_CONFIG         0x60
#define PS2_CMD_TEST_CONTROLLER      0xAA
#define PS2_CMD_TEST_FIRST_PORT      0xAB
#define PS2_CMD_TEST_SECOND_PORT     0xA9

// PS/2 Response Codes
#define PS2_RESP_ACK                 0xFA
#define PS2_RESP_RESEND              0xFE
#define PS2_RESP_ERROR               0xFC
#define PS2_RESP_SELF_TEST_PASSED    0x55
#define PS2_RESP_SELF_TEST_FAILED    0xFC

// All functions give up after a timeout instead of hanging when no
// controller or device answers

// Set up the controller; returns false if there is none or port 1 failed
bool ps2_controller_init(void);

// Send a byte to the device on port 1; false on timeout
bool ps2_write(uint8_t data);

// Wait for and read one byte; -1 on timeout
int ps2_read(void);

// Send a command to the controller; false on timeout
bool ps2_command(uint8_t cmd);

// True if a byte is waiting to be read (does not wait)
bool ps2_has_data(void);

#endif // __KERNEL_DRIVERS_PS2_H__
