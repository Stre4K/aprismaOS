// switch.S (i386)
//
// Only the callee-saved registers (ebx, esi, edi, ebp) need saving: the
// switch is an ordinary function call, so the compiler already preserves
// the rest, and kernel threads do not use the FPU. The interrupt flag is
// not switched: every path into the scheduler runs with interrupts
// disabled and restores the thread's own state when it resumes.

.section .text

// void arch_switch_context(uintptr_t *old_sp, uintptr_t new_sp)
.global arch_switch_context
.type arch_switch_context, @function
arch_switch_context:
    movl 4(%esp), %eax     // old_sp
    movl 8(%esp), %edx     // new_sp

    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl %esp, (%eax)
    movl %edx, %esp

    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.size arch_switch_context, . - arch_switch_context

// First code of every new thread, entered by the ret above with the
// thread's entry function in ebx and its argument in esi
.global arch_thread_start
.type arch_thread_start, @function
arch_thread_start:
    sti
    subl $12, %esp         // keep the stack 16-byte aligned at the call
    pushl %esi             // arg
    calll *%ebx            // entry(arg)
    addl $16, %esp
    calll thread_exit      // entry returned: end the thread (never returns)
    ud2
.size arch_thread_start, . - arch_thread_start
