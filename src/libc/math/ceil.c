// ceil.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-04

#include <math.h>

// Doubles of at least 2^52 have no fractional bits, and below that the
// value fits in a long long, so truncating through it is exact
#define NO_FRACTION 4503599627370496.0  // 2^52

double ceil(double x) {
    if (!(fabs(x) < NO_FRACTION) || x == 0) {
        return x;  // integral already, inf, nan or a signed zero
    }

    double t = (double)(long long)x;  // rounds toward zero
    if (t < x) {
        t += 1.0;
    }
    if (t == 0 && x < 0) {
        return -0.0;  // ceil(-0.5) is -0
    }
    return t;
}
