// serial.h
// COM1 serial port output, used to mirror the kernel console

#ifndef __KERNEL_DRIVERS_SERIAL_H__
#define __KERNEL_DRIVERS_SERIAL_H__

#include <stdbool.h>

// Set up COM1 (115200 baud, 8N1, no interrupts). Returns false when no
// UART answers; output functions then do nothing.
bool serial_init(void);

// True if COM1 was found and is still responding
bool serial_available(void);

// Send one character; '\n' is sent as "\r\n" for terminal programs
void serial_putchar(char c);

void serial_write(const char *s);

#endif // __KERNEL_DRIVERS_SERIAL_H__
