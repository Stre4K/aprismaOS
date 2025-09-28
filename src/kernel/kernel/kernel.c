#include <assert.h>
#include <errno.h>
#include <kernel/gdt.h>
#include <kernel/multiboot.h>
#include <kernel/printk.h>
#include <kernel/tty.h>
#include <string.h>
#include <stdio.h>


void kernel_main(uint32_t magic, uint32_t mb_info_ptr) {
    terminal_initialize();

    if (magic != 0x2BADB002) {
        printk("Bad magic number: %08x\n", magic);
        // Not booted by Multiboot
        while (1);
    }
    print_multiboot_info((multiboot_info_t *)mb_info_ptr);

    init_gdt();
    printk("Loaded GDT\n");

    printk("Hello, kernel TEST World!\n");
    printk("test\n");


    if (EPERM == 1) {
        printk("EPERM\n");
    }
    errno = EOVERFLOW;
    printk("errno: %s\n", strerror(errno));

    perror("perror");

    printk("test %d\n", 1020);

    assert(0 == 0);
    assert(0 == 1);
}
