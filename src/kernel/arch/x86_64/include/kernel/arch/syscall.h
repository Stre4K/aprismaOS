// arch/syscall.h (x86_64)
// How system calls arrive: "int $0x80" with the number in rax, arguments in
// rdi, rsi, rdx (the order of the C calling convention) and the result
// returned in rax.

#ifndef _KERNEL_ARCH_SYSCALL_H
#define _KERNEL_ARCH_SYSCALL_H

#include <kernel/arch/isr.h>
#include <stdint.h>

#define SYSCALL_VECTOR 0x80

#define SYSCALL_NUMBER(r)        ((r)->rax)
#define SYSCALL_ARG0(r)          ((r)->rdi)
#define SYSCALL_ARG1(r)          ((r)->rsi)
#define SYSCALL_ARG2(r)          ((r)->rdx)
#define SYSCALL_SET_RETURN(r, v) ((r)->rax = (uint64_t)(int64_t)(v))

#define REGS_FROM_USER(r)        (((r)->cs & 3) == 3)
#define REGS_PC(r)               ((r)->rip)

// Drop to ring 3 at `entry` with stack pointer `user_sp`; never returns.
// The current address space must contain the user program.
__attribute__((noreturn)) void arch_enter_user(uintptr_t entry, uintptr_t user_sp);

// Kernel stack the CPU switches to when an interrupt arrives from user mode
void arch_set_kernel_stack(uintptr_t top);

#endif
