#include <assert.h>
#include <errno.h>
#include <kernel/tty.h>
#include <stdio.h>
#include <string.h>
#include <kernel/gdt.h>

void kernel_main(void) {
    terminal_initialize();

    init_gdt();
    printf("Loaded GDT\n");


    printf("Hello, kernel TEST World!\n");
    printf("test\n");
    if (EPERM == 1) {
        printf("EPERM\n");
    }
    errno = EOVERFLOW;
    printf("errno: %s\n", strerror(errno));

    perror("perror");

    printf("test %d\n", 1020);

    assert(0 == 0);
    //assert(0 == 1);
}
