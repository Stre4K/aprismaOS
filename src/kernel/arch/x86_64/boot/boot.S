// boot.S (x86_64)
//
// GRUB (Multiboot1) enters _start in 32-bit protected mode with paging off.
// This trampoline checks for long mode, builds temporary page tables, enables
// long mode and jumps to kernel_main in the higher half.
//
// Temporary mappings (2 MiB pages, physical 0..1 GiB) until paging_init:
//   0x0000000000000000 -> 0  identity, for the trampoline and boot data
//   0xFFFF800000000000 -> 0  direct map (PHYS_MAP_BASE), used by P2V()
//   0xFFFFFFFF80000000 -> 0  higher half, where the kernel is linked

#define KERNEL_VMA 0xFFFFFFFF80000000
#define V2P(x) ((x) - KERNEL_VMA)

#define PAGE_PRESENT (1 << 0)
#define PAGE_RW      (1 << 1)
#define PAGE_HUGE    (1 << 7)

#define CR0_PE (1 << 0)
#define CR0_WP (1 << 16)
#define CR0_PG (1 << 31)
#define CR4_PAE (1 << 5)
#define MSR_EFER 0xC0000080
#define EFER_LME (1 << 8)

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10

// Multiboot1 header
.set MB_MAGIC,    0x1BADB002
.set MB_FLAGS,    (1 << 0) | (1 << 1)    // page-align modules, provide memory map
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.section .multiboot, "a"
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM

// Page tables and stack live in the higher-half .bss; the trampoline reaches
// them through their physical addresses with V2P().
.section .bss
.align 4096
boot_pml4:      .skip 4096
boot_pdpt_low:  .skip 4096
boot_pdpt_high: .skip 4096
boot_pd:        .skip 4096

.align 16
.global stack_bottom
stack_bottom:
.skip 16384 // 16 KiB
.global stack_top
stack_top:

// Boot data must be reachable before paging, so it is linked low
.section .boot.data, "aw"
.align 16
boot_gdt:
    .quad 0                    // null
    .quad 0x00AF9A000000FFFF   // 0x08: 64-bit kernel code (L=1)
    .quad 0x00CF92000000FFFF   // 0x10: kernel data
boot_gdt_end:

boot_gdt_ptr:
    .word boot_gdt_end - boot_gdt - 1
    .quad boot_gdt

no_long_mode_msg:
    .asciz "AprismaOS: this CPU does not support 64-bit long mode."

.section .boot.text, "ax"
.code32
.global _start
.type _start, @function
_start:
    cli
    cld
    movl $V2P(stack_top), %esp

    // Keep the Multiboot magic and info pointer in registers that
    // cpuid/rdmsr do not clobber; they become kernel_main's arguments
    movl %eax, %edi
    movl %ebx, %esi

    // CPUID is available if the ID flag (EFLAGS bit 21) can be toggled
    pushfl
    popl %eax
    movl %eax, %ecx
    xorl $(1 << 21), %eax
    pushl %eax
    popfl
    pushfl
    popl %eax
    pushl %ecx
    popfl
    cmpl %eax, %ecx
    je no_long_mode

    // Long mode: extended leaf 0x80000001, EDX bit 29
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb no_long_mode
    movl $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx
    jz no_long_mode

    // Page directory: 512 x 2 MiB pages covering physical 0..1 GiB
    xorl %ecx, %ecx
1:
    movl %ecx, %eax
    shll $21, %eax
    orl $(PAGE_PRESENT | PAGE_RW | PAGE_HUGE), %eax
    movl %eax, V2P(boot_pd)(, %ecx, 8)
    movl $0, V2P(boot_pd) + 4(, %ecx, 8)
    incl %ecx
    cmpl $512, %ecx
    jne 1b

    // Identity map: PML4[0] -> PDPT_low[0] -> PD
    movl $(V2P(boot_pd) + PAGE_PRESENT + PAGE_RW), V2P(boot_pdpt_low)
    movl $(V2P(boot_pdpt_low) + PAGE_PRESENT + PAGE_RW), V2P(boot_pml4)

    // Direct map at 0xFFFF800000000000: PML4[256] -> the same PDPT
    movl $(V2P(boot_pdpt_low) + PAGE_PRESENT + PAGE_RW), V2P(boot_pml4) + 256 * 8

    // Higher half: PML4[511] -> PDPT_high[510] -> PD
    movl $(V2P(boot_pd) + PAGE_PRESENT + PAGE_RW), V2P(boot_pdpt_high) + 510 * 8
    movl $(V2P(boot_pdpt_high) + PAGE_PRESENT + PAGE_RW), V2P(boot_pml4) + 511 * 8

    movl $V2P(boot_pml4), %eax
    movl %eax, %cr3

    movl %cr4, %eax
    orl $CR4_PAE, %eax
    movl %eax, %cr4

    movl $MSR_EFER, %ecx
    rdmsr
    orl $EFER_LME, %eax
    wrmsr

    // Turning on paging with EFER.LME set activates long mode
    movl %cr0, %eax
    orl $(CR0_PG | CR0_WP | CR0_PE), %eax
    movl %eax, %cr0

    lgdt boot_gdt_ptr
    ljmp $GDT_KERNEL_CODE, $long_mode_entry

no_long_mode:
    // Print the message in white on red and stop
    movl $no_long_mode_msg, %esi
    movl $0xB8000, %edi
2:
    lodsb
    testb %al, %al
    jz 3f
    movb $0x4F, %ah
    stosw
    jmp 2b
3:
    hlt
    jmp 3b

.code64
long_mode_entry:
    movw $GDT_KERNEL_DATA, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    // Still running at the low identity address; jump to the higher half
    movabsq $higher_half_entry, %rax
    jmp *%rax

.section .text
higher_half_entry:
    movq $stack_top, %rsp
    xorq %rbp, %rbp

    // kernel_main(uint32_t magic, multiboot_info_t *mbi)
    // The upper halves of 64-bit registers are undefined after the mode
    // switch, so zero-extend the values saved in 32-bit mode
    movl %edi, %edi
    movl %esi, %esi
    call kernel_main

    cli
4:
    hlt
    jmp 4b
