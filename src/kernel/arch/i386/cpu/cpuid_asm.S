.section .text
.global cpuid_supported_check_asm
.type cpuid_supported_check_asm, @function
cpuid_supported_check_asm:
    pushfl                               #;Save EFLAGS
    pushfl                               #;Store EFLAGS
    xorl $0x00200000,(%esp)              #;Invert the ID bit in stored EFLAGS
    popfl                                #;Load stored EFLAGS (with ID bit inverted)
    pushfl                               #;Store EFLAGS again (ID bit may or may not be inverted)
    popl %eax                            #;eax = modified EFLAGS (ID bit may or may not be inverted)
    xor (%esp), %eax                    #;eax = whichever bits were changed
    popfl                                #;Restore original EFLAGS
    andl $0x00200000,%eax                #;eax = zero if ID bit can't be changed, else non-zero
    ret
