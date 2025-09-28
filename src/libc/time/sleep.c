// libc/time/sleep.c
#include <limits.h>
#include <time.h>

// Implementation of sleep() in assembly.
// TODO : Add proper implementation.
// Remove sleep from entire cpu code.
// Current implementation does not sleep as long as defined.
void sleep(uint32_t seconds) {
    volatile uint32_t count = seconds * 100000000;
    if (count > INT_MAX) {
        count = INT_MAX;
    }
    while (count--) {
        __asm__("nop");
    }
}
