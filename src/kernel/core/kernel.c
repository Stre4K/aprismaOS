#include <kernel/multiboot.h>
#include <stdbool.h>
#include <kernel/drivers/serial.h>
#include <kernel/drivers/tty.h>
#include <kernel/printk.h>
#include <kernel/mm/memory_map.h>
#include <kernel/arch/gdt.h>
#include <kernel/arch/idt.h>
#include <kernel/arch/isr.h>
#include <kernel/arch/pic.h>
#include <kernel/arch/timer.h>
#include <kernel/arch/cpuid.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/drivers/ps2.h>

#include <kernel/arch/memlayout.h>
#include <kernel/mm/fault.h>
#include <kernel/mm/paging.h>
#include <kernel/mm/pmm.h>
#include <kernel/mm/heap.h>

// mb_info_phys is the physical address the bootloader passed in EBX
void kernel_main(uint32_t magic, uintptr_t mb_info_phys) {
    disable_interrupts();

    // First, so every message (including early panics) reaches the serial log
    bool have_serial = serial_init();

    multiboot_info_t *mb_info_ptr = P2V(mb_info_phys);

    terminal_initialize();
    printk(have_serial ? "[SERIAL] Console mirrored to COM1\n" : "[SERIAL] No serial port found\n");

    if (magic != 0x2BADB002) {
        printk("Bad magic number: 0x%08x\n", magic);
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

    register_isr_handler(14, page_fault_handler);

    memory_map_detect(mb_info_ptr);
    pmm_init(mb_info_ptr);
    paging_init();
    printk("[PAGING] Enabled\n");

    kheap_init();
    printk("[HEAP] Initialized\n");

    if (cpuid_supported_check()) {
        cpuid_init();
        printk("[CPUID] Loaded\n");
    }

    pic_init();
    pit_init();

    if (ps2_controller_init())
        keyboard_init();


    // Currently unused
    //lapic_init();
    //lapic_timer_init(1, 32);
    //lapic_keyboard_init();

    enable_interrupts();

    // Main loop: interrupt handlers only queue work; it is processed here
    // with interrupts enabled, so shell commands never block the timer
    while (1) {
        keyboard_process();

        // Sleep until the next interrupt unless input arrived meanwhile
        disable_interrupts();
        if (keyboard_has_input())
            enable_interrupts();
        else
            enable_interrupts_and_halt();
    }
}
