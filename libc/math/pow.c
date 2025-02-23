// pow.c
// <math.h>
// Author: Stre4K
// Date: 2025-02-23

#include <math.h>

int pow(int base, int exponent) {
    int result = 1;
    for (int i = 0; i < exponent; i++) {
        result *= base;
    }
    return result;
}
