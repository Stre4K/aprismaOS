// pow.c
// <math.h>
// Author: Stre4K
// Date: 2025-02-23

#include <math.h>

// base^n for a non-negative integer n by repeated squaring
static double pow_int(double base, unsigned long long n) {
    double result = 1.0;

    while (n > 0) {
        if (n & 1) {
            result *= base;
        }
        base *= base;
        n >>= 1;
    }

    return result;
}

// base^exponent. Integer exponents up to 2^31 use repeated squaring; other
// powers use exp(exponent * log(|base|)), whose relative error grows with
// |exponent * log(base)| (up to a few hundred ulp for results near the
// limits of double)
double pow(double base, double exponent) {
    if (exponent == 0 || base == 1.0) {
        return 1.0;  // even for nan
    }
    if (isnan(base) || isnan(exponent)) {
        return NAN;
    }

    if (isinf(exponent)) {
        double magnitude = fabs(base);
        if (magnitude == 1.0) {
            return 1.0;
        }
        return (magnitude > 1.0) == (exponent > 0) ? INFINITY : 0.0;
    }

    int integral = exponent == floor(exponent);

    if (integral && fabs(exponent) <= 2147483648.0) {
        double result = pow_int(base, (unsigned long long)fabs(exponent));
        return exponent < 0 ? 1.0 / result : result;
    }

    if (base < 0 && !integral) {
        return NAN;  // no real result
    }

    // Large integral exponents: the sign depends on whether it is odd
    double sign = (base < 0 && fmod(exponent, 2.0) != 0) ? -1.0 : 1.0;
    return sign * exp(exponent * log(fabs(base)));
}
