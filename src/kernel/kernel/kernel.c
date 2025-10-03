#include <kernel/multiboot.h>
#include <kernel/arch/gdt.h>
#include <kernel/arch/idt.h>
#include <kernel/arch/isr.h>
#include <kernel/arch/cpuid.h>
#include <kernel/arch/lapic.h>
#include <kernel/arch/pic.h>
#include <kernel/arch/portio.h>
#include <kernel/arch/timer.h>
#include <kernel/drivers/tty.h>
#include <kernel/printk.h>
void kernel_main(uint32_t magic, multiboot_info_t *mb_info_ptr) {
    terminal_initialize();

    if (magic != 0x2BADB002) {
        printk("Bad magic number: %08x\n", magic);
        // Not booted by Multiboot
        while (1);
    }
    if (mb_info_ptr == NULL) {
        // No Multiboot info
    }
    //print_multiboot_info((multiboot_info_t *)mb_info_ptr);
    init_gdt();
    printk("[GDT] Loaded\n");

    init_idt();
    printk("[IDT] Loaded\n");


    if (cpuid_supported_check()) {
        cpuid_init();
        printk("[CPUID] Loaded\n");
    }

    //pic_init();
    //pit_init();

    lapic_init();

    lapic_timer_init(1, 32);

    enable_interrupts();

    while(1) {
        continue;
    }
}
