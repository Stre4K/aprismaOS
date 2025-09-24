#include <assert.h>
#include <errno.h>
#include <kernel/gdt.h>
#include <kernel/multiboot.h>
#include <kernel/printk.h>
#include <kernel/tty.h>
#include <stdio.h>
#include <string.h>

void kernel_main(uint32_t magic, uint32_t mb_info_ptr) {
    terminal_initialize();

    if (magic != 0x2BADB002) {
        printk("Bad magic number: %08x\n", magic);
        // Not booted by Multiboot
        while (1);
    }
    print_multiboot_info((multiboot_info_t *)mb_info_ptr);

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
    // assert(0 == 1);
}
