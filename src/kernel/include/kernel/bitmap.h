// bitmap.h
// Author: Stre4k
// Date: 2026-03-08

#ifndef __KERNEL_BITMAP_H__
#define __KERNEL_BITMAP_H__

#include <stdint.h>

void set_bit(uint8_t *bitmap, uint32_t bit);
void clear_bit(uint8_t *bitmap, uint32_t bit);
int test_bit(uint8_t *bitmap, uint32_t bit);


#endif // __KERNEL_BITMAP_H__
