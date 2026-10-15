#include <kernel/multiboot.h>
#include <kernel/drivers/tty.h>
#include <kernel/printk.h>
#include <kernel/mm/memory_map.h>
#include <kernel/arch/gdt.h>
#include <kernel/arch/idt.h>
#include <kernel/arch/isr.h>
#include <kernel/arch/pic.h>
#include <kernel/arch/timer.h>

// Subsystems not yet ported to x86_64 stay i386-only until their port lands
#if defined(__i386__)
#include <kernel/arch/cpuid.h>
#include <kernel/arch/lapic.h>
#include <kernel/arch/portio.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/drivers/ps2.h>
#include <kernel/shell/commands.h>
#include <kernel/mm/fault.h>
#include <kernel/mm/paging.h>
#include <kernel/mm/pmm.h>
#include <kernel/mm/heap.h>
#endif

void kernel_main(uint32_t magic, multiboot_info_t *mb_info_ptr) {
    disable_interrupts();

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

#if defined(__x86_64__)
    printk("[x86_64] Running in 64-bit long mode\n");
#endif

    init_gdt();
    printk("[GDT] Loaded\n");

    // Load the IDT before touching paging so early faults are reported
    // instead of triple faulting
    init_idt();
    printk("[IDT] Loaded\n");

#if defined(__i386__)
    register_isr_handler(14, page_fault_handler);

    memory_map_detect(mb_info_ptr);
    pmm_init(mb_info_ptr);
    paging_init();
    enable_paging(page_directory);

    kheap_init();
    printk("[HEAP] Initialized\n");


    if (cpuid_supported_check()) {
        cpuid_init();
        printk("[CPUID] Loaded\n");
    }
#endif

    pic_init();
    pit_init();

#if defined(__i386__)
    ps2_controller_init();
    keyboard_init();
#endif


    // Currently unused
    //lapic_init();
    //lapic_timer_init(1, 32);
    //lapic_keyboard_init();

    enable_interrupts();

#if defined(__x86_64__)
    // TODO(x86_64): port memory management, keyboard and the shell
    printk("[x86_64] Interrupts enabled\n");
#endif

    while(1) {
        __asm__ volatile("hlt");
    }
}
