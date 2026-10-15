// user.c (x86_64)
// <kernel/arch/syscall.h>

#include <kernel/arch/syscall.h>
#include <kernel/arch/tss.h>

void arch_set_kernel_stack(uintptr_t top)
{
    write_tss_rsp0(top);
}
