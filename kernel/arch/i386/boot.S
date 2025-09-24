# Declare constants for the multiboot header.
.set ALIGN,    1<<0             # align loaded modules on page boundaries
.set MEMINFO,  1<<1             # provide memory map
.set FLAGS,    ALIGN | MEMINFO  # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       # 'magic number' lets bootloader find the header MultiBoot1
#.set MAGIC,    0xE85250D6       # 'magic number' lets bootloader find the header Multiboot2
#.set MAGIC,    0x36D76289       # 'magic number' lets bootloader find the header Multiboot1 and Multiboot2
.set CHECKSUM, -(MAGIC + FLAGS) # checksum of above, to prove we are multiboot

# Declare a header as in the Multiboot Standard.
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# Reserve a stack for the initial thread.
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

# The kernel entry point.
.section .text
.global _start
.type _start, @function
_start:
	movl $stack_top, %esp

    # save Multiboot registers
    movl %eax, %edi
    movl %ebx, %esi

	# Call the global constructors.
	call _init

	# Transfer control to the main kernel.
    push %esi   # pointer to multiboot_info
    push %edi   # multiboot magic
	call kernel_main
    add $8, %esp    # clean up stack (2 arguments * 4 bytes)

	# Hang if kernel_main unexpectedly returns.
	cli
1:	hlt
	jmp 1b
.size _start, . - _start
