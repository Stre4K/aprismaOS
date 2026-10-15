// fmod.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-09

#include <math.h>

// Remainder of x / y with the sign of x, computed exactly: subtract y scaled
// to the remainder's exponent until the remainder is smaller than |y|. Each
// subtraction is exact because the scaled y is within a factor of two of the
// remainder, and each step lowers the remainder's exponent.
double fmod(double x, double y) {
    if (isnan(x) || isnan(y) || isinf(x) || y == 0) {
        return NAN;
    }

    double ax = fabs(x);
    double ay = fabs(y);

    while (ax >= ay) {
        int ex, ey;
        frexp(ax, &ex);
        frexp(ay, &ey);

        double d = ldexp(ay, ex - ey);  // same exponent as ax
        if (d > ax) {
            d = ldexp(ay, ex - ey - 1);
        }
        ax -= d;
    }

    return x < 0 ? -ax : ax;  // also gives -0 for exact multiples of a negative x
}
