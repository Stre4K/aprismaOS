// bitmap.c
// <kernel/bitmap.h>
// Author: Stre4k
// Date: 2026-03-08

#include <stdint.h>

void set_bit(uint8_t *bitmap, uint32_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

void clear_bit(uint8_t *bitmap, uint32_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}


int test_bit(uint8_t *bitmap, uint32_t bit) {
    return (bitmap[bit / 8] >> (bit % 8)) & 1;
    //return bitmap[bit / 8] & (1 << (bit % 8));
}

