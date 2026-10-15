// enter.S (x86_64)

#define USER_DATA_SELECTOR (0x18 | 3)  // GDT_USER_DATA, RPL 3
#define USER_CODE_SELECTOR (0x20 | 3)  // GDT_USER_CODE, RPL 3
#define RFLAGS_IF          0x202       // interrupts enabled (bit 1 is always set)

.section .text

// void arch_enter_user(uintptr_t entry, uintptr_t user_sp)
//
// Build the frame iretq expects for a return to ring 3 and use it: the CPU
// loads CS/SS with the user selectors, switches to the user stack and
// continues at `entry` in user mode.
.global arch_enter_user
.type arch_enter_user, @function
arch_enter_user:
    pushq $USER_DATA_SELECTOR   // SS
    pushq %rsi                  // RSP
    pushq $RFLAGS_IF            // RFLAGS
    pushq $USER_CODE_SELECTOR   // CS
    pushq %rdi                  // RIP

    // Don't leak kernel values to user mode
    xorl %eax, %eax
    xorl %ebx, %ebx
    xorl %ecx, %ecx
    xorl %edx, %edx
    xorl %esi, %esi
    xorl %edi, %edi
    xorl %ebp, %ebp
    xorl %r8d, %r8d
    xorl %r9d, %r9d
    xorl %r10d, %r10d
    xorl %r11d, %r11d
    xorl %r12d, %r12d
    xorl %r13d, %r13d
    xorl %r14d, %r14d
    xorl %r15d, %r15d
    iretq
.size arch_enter_user, . - arch_enter_user
