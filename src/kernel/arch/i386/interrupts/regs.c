// regs.c
// <kernel/arch/isr.h>

#include <kernel/arch/isr.h>
#include <kernel/arch/cpu.h>
#include <kernel/printk.h>

void regs_dump(regs_t *r)
{
    printk("EIP=%x CS=%x EFLAGS=%x ERR=%x\n", r->eip, r->cs, r->eflags, r->err_code);
    printk("EAX=%x EBX=%x ECX=%x EDX=%x\n", r->eax, r->ebx, r->ecx, r->edx);
    printk("ESI=%x EDI=%x EBP=%x CR2=%x\n", r->esi, r->edi, r->ebp, read_cr2());
}
