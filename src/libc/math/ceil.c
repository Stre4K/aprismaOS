// ceil.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-04

#include <math.h>

double ceil(double x) {
    int int_x = (int)x;  // ceil(x) = int(x);

    if (x > 0 && int_x != x) {
        return int_x + 1;
    }
    return int_x;
}
