// memory_map.h
// Author: Stre4K
// Date: 2026-03-08


#ifndef __KERNEL_MM_MEMORY_MAP_H__
#define __KERNEL_MM_MEMORY_MAP_H__

#include <kernel/multiboot.h>

void memory_map_detect(multiboot_info_t *mbi);

#endif // __KERNEL_MM_MEMORY_MAP_H__
