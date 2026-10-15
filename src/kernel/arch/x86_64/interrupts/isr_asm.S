// isr_asm.S (x86_64)
//
// Entry stubs for vectors 0-47 (CPU exceptions and PIC IRQs).
//
// In long mode the CPU always pushes SS, RSP, RFLAGS, CS and RIP (plus an
// error code for some exceptions) after aligning RSP to 16 bytes. Each stub
// pushes a fake error code if needed and the vector number, then the common
// path saves every general purpose register (there is no pusha in 64-bit
// mode) and calls isr_common_handler(regs_t *). The resulting layout matches
// regs_t in <kernel/arch/isr.h>.

.section .text

.macro ISR_NOERR n
.global isr\n
isr\n:
    pushq $0            // fake error code so every frame looks the same
    pushq $\n           // vector number
    jmp isr_common
.endm

.macro ISR_ERR n
.global isr\n
isr\n:
    pushq $\n           // CPU already pushed the error code
    jmp isr_common
.endm

isr_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    cld
    // RSP is 16-byte aligned here: the CPU aligned it before pushing its
    // 5-qword frame, and the stub plus the 15 registers add 17 qwords
    movq %rsp, %rdi
    call isr_common_handler

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    addq $16, %rsp      // drop vector number and error code
    iretq

// CPU exceptions 0-31; vectors 8, 10-14, 17, 21 and 30 push an error code
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

// PIC IRQs 0-15, remapped to vectors 32-47
.irp n, 32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
ISR_NOERR \n
.endr

.section .rodata
.align 8
.global isr_stub_table
isr_stub_table:
.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
    .quad isr\n
.endr
