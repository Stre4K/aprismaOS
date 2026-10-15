// round.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-14

#include <math.h>

// Doubles of at least 2^52 have no fractional bits, and below that the
// value fits in a long long, so truncating through it is exact
#define NO_FRACTION 4503599627370496.0  // 2^52

// Round to the nearest integer, halfway cases away from zero
double round(double x) {
    if (!(fabs(x) < NO_FRACTION) || x == 0) {
        return x;  // integral already, inf, nan or a signed zero
    }

    double t = (double)(long long)x;  // rounds toward zero
    // x - t is exact here, so 0.49999999999999994 does not round up
    if (x - t >= 0.5) {
        t += 1.0;
    } else if (t - x >= 0.5) {
        t -= 1.0;
    }
    if (t == 0 && x < 0) {
        return -0.0;  // round(-0.4) is -0
    }
    return t;
}
