// abs.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-13

#include <math.h>

int abs(int x) {
    if (x < 0) {
        return -x;
    }
    return x;
}

long labs(long x) {
    if (x < 0) {
        return -x;
    } else {
        return x;
    }
}
