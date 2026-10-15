// start.S: entry point of every user program

.section .text
.global _start
.type _start, @function
_start:
    xorl %ebp, %ebp        // marks the outermost stack frame
    andq $-16, %rsp        // ABI stack alignment for the call
    callq main
    movl %eax, %edi        // main's return value is the exit code
    callq exit
    ud2                    // exit never returns
.size _start, . - _start
