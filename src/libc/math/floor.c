// floor.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-03

#include <math.h>

double floor(double x) {
    int int_x = (int)x;  // floor(x) = int(x;

    if (x < 0 && int_x != x) {
        return int_x - 1;
    }
    return int_x;
}
