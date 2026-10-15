// sqrt.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-11

#include <math.h>

// Newton's method on the mantissa only: x = m * 2^e with e made even (or a
// multiple of 3 for cbrt) keeps m in a small range around 1, so a fixed
// starting guess converges to full precision in a few steps for any x

double sqrt(double x) {
    if (isnan(x) || x < 0) {
        return NAN;
    }
    if (x == 0 || isinf(x)) {
        return x;  // keeps the sign of -0
    }

    int e;
    double m = frexp(x, &e);  // m in [0.5, 1)
    if (e & 1) {              // make e even: m in [0.5, 2)
        m *= 2;
        e--;
    }

    double g = 0.5 * (1.0 + m);  // within 6% of sqrt(m)
    for (int i = 0; i < 6; i++) {
        g = 0.5 * (g + m / g);
    }
    return ldexp(g, e / 2);
}

double cbrt(double x) {
    if (isnan(x) || isinf(x) || x == 0) {
        return x;
    }
    if (x < 0) {
        return -cbrt(-x);
    }

    int e;
    double m = frexp(x, &e);  // m in [0.5, 1)
    int r = e % 3;
    if (r < 0) {
        r += 3;
    }
    m = ldexp(m, r);          // make e a multiple of 3: m in [0.5, 4)
    e -= r;

    // Newton step written as a small correction to g, so its rounding error
    // stays in the correction instead of in g itself
    double g = 1.0;
    for (int i = 0; i < 8; i++) {
        g += (m / (g * g) - g) / 3.0;
    }
    return ldexp(g, e / 3);
}
