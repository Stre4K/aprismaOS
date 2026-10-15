// panic.h
// Author: Stre4k
// Date: 2026-03-07

#ifndef __KERNEL_PANIC_H__
#define __KERNEL_PANIC_H__

__attribute__((noreturn)) void panic(const char *msg);

#endif // __KERNEL_PANIC_H__
