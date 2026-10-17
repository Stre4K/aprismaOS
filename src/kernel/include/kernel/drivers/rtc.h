// rtc.h
// The CMOS real-time clock, the battery-backed clock of a PC. It keeps
// the date and time (UTC here) while the machine is off; the kernel reads
// it once at boot (see <kernel/time.h>).

#ifndef __KERNEL_DRIVERS_RTC_H__
#define __KERNEL_DRIVERS_RTC_H__

#include <kernel/abi/time.h>
#include <stdbool.h>
#include <stdint.h>

// Read the date and time; false if the clock gives nonsense
bool rtc_read(date_t *out);

// The raw clock registers, in this order
enum { RTC_SECONDS, RTC_MINUTES, RTC_HOURS, RTC_DAY, RTC_MONTH, RTC_YEAR, RTC_CENTURY, RTC_FIELDS };

// Turn raw register values into a date. status_b says how they are
// stored: bit 2 set for binary (otherwise BCD, "0x59" for 59), bit 1 set
// for a 24-hour clock (otherwise 12 hours with bit 7 of the hour for PM).
// A century register of 0 (missing) means 19xx for years from 70, else
// 20xx. False if a field is out of range. Unit tested on the host.
bool rtc_decode(const uint8_t raw[RTC_FIELDS], uint8_t status_b, date_t *out);

#endif // __KERNEL_DRIVERS_RTC_H__
