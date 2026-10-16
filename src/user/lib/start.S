// start.S: entry point of every user program

.section .text
.global _start
.type _start, @function
// The kernel starts us with the System V layout: rsp points at argc,
// followed by the argv pointers
_start:
    xorl %ebp, %ebp        // marks the outermost stack frame
    movq (%rsp), %rdi      // main(argc,
    leaq 8(%rsp), %rsi     //      argv)
    andq $-16, %rsp        // ABI stack alignment for the call
    callq main
    movl %eax, %edi        // main's return value is the exit code
    callq exit
    ud2                    // exit never returns
.size _start, . - _start
