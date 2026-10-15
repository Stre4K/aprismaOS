// switch.S (x86_64)
//
// Only the callee-saved registers (rbx, rbp, r12-r15) need saving: the
// switch is an ordinary function call, so the compiler already preserves
// the rest, and the kernel uses no SSE/FPU state. The interrupt flag is
// not switched: every path into the scheduler runs with interrupts
// disabled and restores the thread's own state when it resumes.

.section .text

// void arch_switch_context(uintptr_t *old_sp, uintptr_t new_sp)
.global arch_switch_context
.type arch_switch_context, @function
arch_switch_context:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)
    movq %rsi, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    ret
.size arch_switch_context, . - arch_switch_context

// First code of every new thread, entered by the ret above with the
// thread's entry function in r12 and its argument in r13
.global arch_thread_start
.type arch_thread_start, @function
arch_thread_start:
    sti
    movq %r13, %rdi
    callq *%r12
    callq thread_exit      // entry returned: end the thread (never returns)
    ud2
.size arch_thread_start, . - arch_thread_start
