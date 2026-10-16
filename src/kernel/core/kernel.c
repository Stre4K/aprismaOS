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
#include <kernel/fs.h>
#include <kernel/sched.h>
#include <kernel/console.h>
#ifdef CONFIG_USERMODE
#include <kernel/arch/fpu.h>
#include <kernel/proc.h>
#endif
#include <kernel/shell/shell.h>

#ifdef CONFIG_USERMODE
// Run the user-space shell (/bin/sh from the ramdisk) until it exits; the
// kernel shell takes over after that
static void run_user_shell(void) {
    if (process_find_program("sh") == NULL)
        return;  // no ramdisk, or no shell in it

    printk("[SHELL] Starting /bin/sh (type 'exit' for the kernel shell)\n");
    process_t *sh = process_spawn("sh");
    if (sh == NULL) {
        printk("[SHELL] Cannot start /bin/sh\n");
        return;
    }
    int code = process_wait(sh);
    printk("[SHELL] /bin/sh exited with code %d\n", code);
}
#endif

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

    int fs_entries = fs_init(mb_info_ptr);
    if (fs_entries >= 0)
        printk("[FS] Ramdisk mounted: %d files and directories\n", fs_entries);
    else
        printk("[FS] No ramdisk found\n");

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

#ifdef CONFIG_SCHED
    // From here on this code runs as thread 0 ("main"); the timer starts
    // preempting it as soon as interrupts are enabled
    sched_init();
    printk("[SCHED] Preemptive scheduler started\n");
#endif

#ifdef CONFIG_USERMODE
    // User programs may use x87/SSE; each process gets its own register state
    arch_fpu_init();
    printk("[FPU] SSE enabled; x87/SSE state saved per process\n");
#endif

    enable_interrupts();

    // The console thread handles the keyboard and line editing; this thread
    // becomes the shell, running one entered line at a time
    console_start();

#ifdef CONFIG_USERMODE
    run_user_shell();
#endif
    printk("[SHELL] Kernel shell ready, type 'help' for commands\n");

    char line[CONSOLE_LINE_MAX + 1];
    while (1) {
        long n = console_read(line, CONSOLE_LINE_MAX);
        if (n <= 0)
            continue;  // Ctrl+D at the shell prompt: nothing to do
        if (line[n - 1] == '\n')
            n--;
        line[n] = '\0';
        shell(line);
    }
}
