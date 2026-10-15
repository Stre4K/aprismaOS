// log.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-06

#include <math.h>

// ln 2 split into a high part with trailing zero bits (so e * LN2_HI is
// exact) and the remainder, as in fdlibm
#define LN2_HI 6.93147180369123816490e-01
#define LN2_LO 1.90821492927058770002e-10
#define SQRT1_2 0.70710678118654752440

// Natural logarithm: write x = m * 2^e with m in [sqrt(1/2), sqrt(2)), so
// log(x) = e * ln2 + log(1 + f) with f = m - 1. With s = f / (2 + f),
// log(1 + f) = 2 atanh(s) = 2s + 2s * R, R = s^2/3 + s^4/5 + ..., which
// converges quickly because |s| <= 0.172. Using 2s = f - s*f this becomes
// f - s * (f - 2R): f is exact, so rounding only touches the small
// correction term (the same arrangement as fdlibm).
double log(double x) {
    if (isnan(x) || x < 0) {
        return NAN;
    }
    if (x == 0) {
        return -INFINITY;
    }
    if (isinf(x)) {
        return x;
    }

    int e;
    double m = frexp(x, &e);  // m in [0.5, 1)
    if (m < SQRT1_2) {
        m *= 2;
        e--;
    }

    double f = m - 1.0;  // exact
    double s = f / (2.0 + f);
    double s2 = s * s;

    // R = s^2/3 + s^4/5 + ... + s^24/25
    double r = 0.0;
    for (int k = 12; k >= 1; k--) {
        r = (r + 1.0 / (2 * k + 1)) * s2;
    }

    double log_m = f - s * (f - 2.0 * r);
    return e * LN2_HI + (log_m + e * LN2_LO);
}

// Base-10 logarithm: log10(x) = ln(x) / ln(10)
double log10(double x) {
    return log(x) * 0.43429448190325182765;  // 1 / ln(10)
}
