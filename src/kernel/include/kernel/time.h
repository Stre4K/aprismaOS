// time.h
// The time of day. The real-time clock is read once at boot; after that
// the time is that moment plus the timer ticks since, so reading it is
// cheap and it never goes backwards. UTC, in seconds or milliseconds
// since 1970-01-01 (see <kernel/abi/time.h> for dates).

#ifndef __KERNEL_TIME_H__
#define __KERNEL_TIME_H__

#include <kernel/abi/time.h>
#include <stdbool.h>
#include <stdint.h>

// Read the real-time clock; false if it could not be read (the time then
// counts from 1970-01-01)
bool time_init(void);

int64_t time_now_ms(void);
uint64_t time_now(void);  // seconds

#endif // __KERNEL_TIME_H__
