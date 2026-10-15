/* x86_64 kernel layout.
 *
 * GRUB loads the image at 1 MiB physical and enters _start in 32-bit
 * protected mode. The .boot sections are linked at its physical address so the
 * trampoline can run before paging. Everything else is linked in the higher
 * half at KERNEL_VMA + physical address and loaded right after them.
 */
ENTRY(_start)

KERNEL_VMA = 0xFFFFFFFF80000000;

SECTIONS
{
    . = 1M;
    kernel_start_phys = .;

    .boot.text ALIGN(4K) :
    {
        KEEP(*(.multiboot))
        *(.boot.text)
    }

    .boot.data ALIGN(4K) :
    {
        *(.boot.data)
    }

    . += KERNEL_VMA;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA)
    {
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA)
    {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA)
    {
        *(.data .data.*)
    }

    /* Shell command table */
    .shell_commands ALIGN(4K) : AT(ADDR(.shell_commands) - KERNEL_VMA)
    {
        __start_shell_commands = .;
        KEEP(*(.shell_commands*))
        __stop_shell_commands = .;
    }

    .signature ALIGN(4K) : AT(ADDR(.signature) - KERNEL_VMA)
    {
        KEEP(*(.signature))
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA)
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    /* mark kernel end: must stay after the last loaded section */
    . = ALIGN(4K);
    kernel_end = .;
    kernel_end_phys = kernel_end - KERNEL_VMA;

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.comment)
        *(.note*)
    }
}
