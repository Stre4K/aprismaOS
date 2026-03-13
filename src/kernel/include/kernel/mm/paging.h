// paging.h
// Author: Stre4k
// Date: 2026-03-08


#ifndef __KERNEL_MM_PAGING_H__
#define __KERNEL_MM_PAGING_H__

#include <stdint.h>

extern uint32_t page_directory[1024];

void paging_init(void);
void enable_paging(uint32_t *pd);
void map_page(uint32_t virt, uint32_t phys);

uint32_t get_physaddr(uint32_t virt);

void dump_mapping(uint32_t virt);
void dump_paging(uint32_t virt);
void test_paging(void);
#endif // __KERNEL_MM_PAGING_H__
