// regs.c
// <kernel/arch/isr.h>

#include <kernel/arch/isr.h>
#include <kernel/arch/cpu.h>
#include <kernel/printk.h>

void regs_dump(regs_t *r)
{
    printk("EIP=0x%x CS=0x%x EFLAGS=0x%x ERR=0x%x\n", r->eip, r->cs, r->eflags, r->err_code);
    printk("EAX=0x%x EBX=0x%x ECX=0x%x EDX=0x%x\n", r->eax, r->ebx, r->ecx, r->edx);
    printk("ESI=0x%x EDI=0x%x EBP=0x%x CR2=0x%x\n", r->esi, r->edi, r->ebp, read_cr2());
}
