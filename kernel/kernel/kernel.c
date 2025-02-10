#include <errno.h>
#include <kernel/tty.h>
#include <stdio.h>
#include <string.h>

void kernel_main(void) {
    terminal_initialize();
    printf("Hello, kernel TEST World!\n");
    printf("test\n");
    if (EPERM == 1) {
        printf("EPERM\n");
    }
    errno = EOVERFLOW;
    printf("errno: %s\n", strerror(errno));
}
