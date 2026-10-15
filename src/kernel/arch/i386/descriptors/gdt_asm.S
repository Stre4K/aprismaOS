.section .text
.global gdt_flush

gdt_flush:
    movl 4(%esp), %eax
    lgdt (%eax)

    # Setup Segement registers with new data segment selector (0x10)
    movw $0x10, %ax # Load selector 0x10 (kernel data segment) into AX
    movw %ax, %ds   # Data segment
    movw %ax, %es   # Extra segment
    movw %ax, %fs   # Extra segment
    movw %ax, %gs   # Extra segment
    movw %ax, %ss   # Stack segment
    ljmp $0x08, $flush # 0x08 = kernel code segment selector, # Far jump to reload CS
flush:
    ret
