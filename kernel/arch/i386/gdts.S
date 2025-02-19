.section .text
.global gdt_flush

gdt_flush:
    movl 4(%esp), %eax
    lgdt (%eax)

    # Setup Segement registers with new data segment selector (0x10)
    movw %ax, 0x10
    movw %ds, %ax   # Data segment
    movw %es, %ax   # Extra segment
    movw %fs, %ax   # Extra segment
    movw %gs, %ax   # Extra segment
    movw %ss, %ax   # Stack segment
    ljmp $0x08, $flush
flush:
    ret
