// fabs.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-05

#include <math.h>

double fabs(double x) {
    if (x < 0) {
        return -x;
    }
    return x;
}
