// regs.c (x86_64)
// <kernel/arch/isr.h>

#include <kernel/arch/isr.h>
#include <kernel/arch/cpu.h>
#include <kernel/printk.h>

void regs_dump(regs_t *r)
{
    printk("RIP=0x%llx CS=0x%llx RFLAGS=0x%llx ERR=0x%llx\n", r->rip, r->cs, r->rflags, r->err_code);
    printk("RSP=0x%llx SS=0x%llx CR2=0x%llx\n", r->rsp, r->ss, read_cr2());
    printk("RAX=0x%llx RBX=0x%llx RCX=0x%llx\n", r->rax, r->rbx, r->rcx);
    printk("RDX=0x%llx RSI=0x%llx RDI=0x%llx\n", r->rdx, r->rsi, r->rdi);
    printk("RBP=0x%llx R8=0x%llx R9=0x%llx\n", r->rbp, r->r8, r->r9);
}
