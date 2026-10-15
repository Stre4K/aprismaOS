// cpuid_asm.S (x86_64)

.section .text
.global cpuid_supported_check_asm
.type cpuid_supported_check_asm, @function

// Every CPU that can run long mode supports CPUID (boot.S already used it)
cpuid_supported_check_asm:
    movl $1, %eax
    ret
.size cpuid_supported_check_asm, . - cpuid_supported_check_asm
