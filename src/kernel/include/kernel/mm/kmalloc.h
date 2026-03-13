// kmalloc.h
// Author: Stre4k
// Date: 2026-03-09

#ifndef __KERNEL_MM_KMALLOC_H__
#define __KERNEL_MM_KMALLOC_H__

#include <stdint.h>

void heap_init();
void* kmalloc(uint32_t size);

#endif // __KERNEL_MM_KMALLOC_H__
