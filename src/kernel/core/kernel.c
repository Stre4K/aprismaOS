#include <kernel/multiboot.h>
#include <kernel/arch/gdt.h>
#include <kernel/arch/idt.h>
#include <kernel/arch/isr.h>
#include <kernel/arch/cpuid.h>
#include <kernel/arch/lapic.h>
#include <kernel/arch/pic.h>
#include <kernel/arch/portio.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/arch/timer.h>
#include <kernel/drivers/tty.h>
#include <kernel/printk.h>
#include <kernel/shell/commands.h>
#include <kernel/mm/memory_map.h>
#include <kernel/mm/fault.h>
#include <kernel/mm/paging.h>
#include <kernel/mm/memory_map.h>
#include <kernel/mm/pmm.h>
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
    multiboot_init(mb_info_ptr);
    memory_map_detect(mb_info_ptr);
    printk("address of multiboot_info: %p\n", mb_info_ptr);
    //print_multiboot_info((multiboot_info_t *)mb_info_ptr);
    init_gdt();
    printk("[GDT] Loaded\n");

    memory_map_detect(mb_info_ptr);
    pmm_init(mb_info_ptr);
    paging_init();
    enable_paging(page_directory);


    init_idt();
    printk("[IDT] Loaded\n");

    register_isr_handler(14, page_fault_handler);


    if (cpuid_supported_check()) {
        cpuid_init();
        printk("[CPUID] Loaded\n");
    }

    pic_init();
    pit_init();
    keyboard_init();

    init_shell_commands();

    // Currently unused
    //lapic_init();
    //lapic_timer_init(1, 32);
    //lapic_keyboard_init();

    enable_interrupts();

    while(1) {
        continue;
    }
}
