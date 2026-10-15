// gdt_asm.S (x86_64)

.section .text
.global gdt_flush
.type gdt_flush, @function

// void gdt_flush(struct gdt_ptr_struct *ptr)
gdt_flush:
    lgdt (%rdi)

    // Reload data segments with the kernel data selector (0x10)
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    // There is no far jump with an immediate in long mode; reload CS by
    // returning to our caller through a far return with the kernel code selector
    popq %rdi
    pushq $0x08
    pushq %rdi
    lretq
.size gdt_flush, . - gdt_flush
