// exp.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-06

#include <math.h>

// ln 2 split into a high part with trailing zero bits (so k * LN2_HI is
// exact) and the remainder, as in fdlibm
#define LN2_HI  6.93147180369123816490e-01
#define LN2_LO  1.90821492927058770002e-10
#define INV_LN2 1.44269504088896338700e+00

#define EXP_OVERFLOW   709.782712893383973096   // above this e^x is inf
#define EXP_UNDERFLOW -745.13321910194110842    // below this e^x is 0

// e^x: write x = k * ln2 + r with |r| <= ln2 / 2, so e^x = 2^k * e^r, and
// compute e^r with a short Taylor series
double exp(double x) {
    if (isnan(x)) {
        return x;
    }
    if (x > EXP_OVERFLOW) {
        return INFINITY;
    }
    if (x < EXP_UNDERFLOW) {
        return 0.0;
    }

    int k = (int)(x * INV_LN2 + (x < 0 ? -0.5 : 0.5));
    double r = (x - k * LN2_HI) - k * LN2_LO;

    // 1 + r(1 + r/2(1 + r/3(...))) up to r^18 / 18!, far below one ulp
    // for |r| <= 0.35
    double p = 1.0;
    for (int n = 18; n >= 1; n--) {
        p = 1.0 + r * p / n;
    }

    return ldexp(p, k);
}
