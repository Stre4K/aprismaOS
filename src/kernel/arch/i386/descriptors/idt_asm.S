/* arch/i386/descriptors/idt_asm.S */
.section .text
.global idt_load
.type idt_load, @function
idt_load:
    movl 4(%esp), %eax   /* address of idt_ptr passed */
    lidt (%eax)
    ret

