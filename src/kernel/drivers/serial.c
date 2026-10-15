// serial.c
// <kernel/drivers/serial.h>
//
// Polled output on the first serial port (a 16550-compatible UART at
// 0x3F8). Nothing waits forever: a missing port is detected at init with a
// loopback test, and a transmitter that stops responding disables output.

#include <kernel/drivers/serial.h>
#include <kernel/arch/portio.h>
#include <stdbool.h>
#include <stdint.h>

#define COM1 0x3F8

// UART registers, as offsets from the base port
#define UART_DATA        0  // transmit/receive buffer (divisor low byte when DLAB=1)
#define UART_INT_ENABLE  1  // interrupt enable (divisor high byte when DLAB=1)
#define UART_FIFO_CTRL   2
#define UART_LINE_CTRL   3
#define UART_MODEM_CTRL  4
#define UART_LINE_STATUS 5

#define LCR_DLAB      0x80  // divisor latch access
#define LCR_8N1       0x03  // 8 data bits, no parity, 1 stop bit
#define FCR_ENABLE    0xC7  // enable and clear FIFOs, 14-byte threshold
#define MCR_NORMAL    0x0F  // DTR, RTS, OUT1, OUT2
#define MCR_LOOPBACK  0x1E  // loopback mode for the self-test
#define LSR_TX_EMPTY  0x20  // transmit holding register empty

#define TX_TIMEOUT 100000   // status polls before giving up on one character

static bool serial_ok;

bool serial_init(void) {
    outb(COM1 + UART_INT_ENABLE, 0x00);          // polled, no interrupts
    outb(COM1 + UART_LINE_CTRL, LCR_DLAB);
    outb(COM1 + UART_DATA, 0x01);                // divisor 1: 115200 baud
    outb(COM1 + UART_INT_ENABLE, 0x00);
    outb(COM1 + UART_LINE_CTRL, LCR_8N1);
    outb(COM1 + UART_FIFO_CTRL, FCR_ENABLE);

    // In loopback mode the UART receives what it sends; without a UART the
    // read returns floating bus values instead
    outb(COM1 + UART_MODEM_CTRL, MCR_LOOPBACK);
    outb(COM1 + UART_DATA, 0xAE);
    if (inb(COM1 + UART_DATA) != 0xAE) {
        serial_ok = false;
        return false;
    }

    outb(COM1 + UART_MODEM_CTRL, MCR_NORMAL);
    serial_ok = true;
    return true;
}

bool serial_available(void) {
    return serial_ok;
}

static void serial_send(char c) {
    for (int i = 0; i < TX_TIMEOUT; i++) {
        if (inb(COM1 + UART_LINE_STATUS) & LSR_TX_EMPTY) {
            outb(COM1 + UART_DATA, (uint8_t)c);
            return;
        }
    }
    serial_ok = false;  // stuck transmitter: stop slowing down every printk
}

void serial_putchar(char c) {
    if (!serial_ok) {
        return;
    }
    if (c == '\n') {
        serial_send('\r');
    }
    serial_send(c);
}

void serial_write(const char *s) {
    while (*s) {
        serial_putchar(*s++);
    }
}
