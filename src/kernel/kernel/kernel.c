#include <kernel/multiboot.h>
#include <kernel/arch/gdt.h>
#include <kernel/arch/idt.h>
#include <kernel/arch/isr.h>
#include <kernel/drivers/tty.h>
#include <kernel/printk.h>

void kernel_main(uint32_t magic, multiboot_info_t *mb_info_ptr) {
    terminal_initialize();

    if (magic != 0x2BADB002) {
        printk("Bad magic number: %08x\n", magic);
        // Not booted by Multiboot
        while (1);
    }
    print_multiboot_info((multiboot_info_t *)mb_info_ptr);

    init_gdt();
    printk("[GDT] Loaded\n");

    init_idt();
    printk("[IDT] Loaded\n");

    enable_interrupts();

    asm volatile("int $0x1F");
    while (1);
}
