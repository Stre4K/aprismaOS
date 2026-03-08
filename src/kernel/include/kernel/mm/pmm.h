// pmm.h
// Author: Stre4k
// Date: 2026-03-08

#ifndef __KERNEL_MM_PMM_H__
#define __KERNEL_MM_PMM_H__

#include <stdint.h>
#include <kernel/multiboot.h>

void pmm_init(multiboot_info_t *mbi);
void *pmm_alloc_page();
void pmm_free_page(void *page);
uint32_t pmm_total_pages();
uint32_t pmm_free_pages();


#endif // __KERNEL_MM_PMM_H__


