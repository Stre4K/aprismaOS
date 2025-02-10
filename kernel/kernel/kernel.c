#include <kernel/tty.h>
#include <stdio.h>
#include <sys/errno.h>

void kernel_main(void) {
    terminal_initialize();
    printf("Hello, kernel TEST World!\n");
    printf("test\n");
    if (EPERM == 1) {
        printf("EPERM\n");
    }
}
