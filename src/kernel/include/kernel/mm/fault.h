// fault.h
// Author: Stre4k
// Date: 2026-03-08

#ifndef __KERNEL_MM_PAGEFAULT_H__
#define __KERNEL_MM_PAGEFAULT_H__

#include <kernel/arch/isr.h>

void page_fault_handler(regs_t* regs);

#endif // __KERNEL_MM_PAGEFAULT_H__
