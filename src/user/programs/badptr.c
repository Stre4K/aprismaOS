// badptr: the kernel must reject bad arguments with an error, not crash
#include "ulib.h"

int main(void)
{
    int failures = 0;

    if (write(1, (const void *)0xFFFFFFFF80100000UL, 16) != -EFAULT)
        failures++;  // kernel memory
    if (write(1, (const void *)0, 1) != -EFAULT)
        failures++;  // NULL
    if (write(1, (const void *)0x500000000UL, 8) != -EFAULT)
        failures++;  // unmapped user address
    if (write(1, "x", (size_t)-1) != -EFAULT)
        failures++;  // range wrapping around
    if (write(7, "x", 1) != -EBADF)
        failures++;  // no such file descriptor
    if (syscall3(999, 0, 0, 0) != -ENOSYS)
        failures++;  // no such system call

    print(failures ? "badptr: FAILED\n" : "badptr: ok\n");
    return failures;
}
