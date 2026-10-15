// regs.c (x86_64)
// <kernel/arch/isr.h>

#include <kernel/arch/isr.h>
#include <kernel/arch/cpu.h>
#include <kernel/printk.h>

void regs_dump(regs_t *r)
{
    printk("RIP=%llx CS=%llx RFLAGS=%llx ERR=%llx\n", r->rip, r->cs, r->rflags, r->err_code);
    printk("RSP=%llx SS=%llx CR2=%llx\n", r->rsp, r->ss, read_cr2());
    printk("RAX=%llx RBX=%llx RCX=%llx\n", r->rax, r->rbx, r->rcx);
    printk("RDX=%llx RSI=%llx RDI=%llx\n", r->rdx, r->rsi, r->rdi);
    printk("RBP=%llx R8=%llx R9=%llx\n", r->rbp, r->r8, r->r9);
}
