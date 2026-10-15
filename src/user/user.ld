/* Layout of AprismaOS user programs (x86_64): linked at USER_START. Each
   segment starts on its own page, because the kernel's ELF loader maps
   whole pages per segment. */
ENTRY(_start)

SECTIONS
{
    . = 0x400000;

    .text ALIGN(4K) :
    {
        *(.text .text.*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) :
    {
        *(.data .data.*)
    }

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.comment)
        *(.note*)
    }
}
