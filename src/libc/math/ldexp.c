// ldexp.c
// <math.h>

#include <math.h>
#include <stdint.h>

// 2^n for n in the normal exponent range [-1022, 1023]
static double pow2(int n) {
    union { uint64_t u; double d; } bits = { (uint64_t)(n + 1023) << 52 };
    return bits.d;
}

// x * 2^exp, overflowing to infinity and underflowing to zero as needed
double ldexp(double x, int exponent) {
    // Scale in steps that stay representable; the result saturates long
    // before the loops run more than a few times
    while (exponent > 1023) {
        x *= pow2(1023);
        exponent -= 1023;
        if (isinf(x)) return x;
    }
    while (exponent < -1022) {
        x *= pow2(-1022);
        exponent += 1022;
        if (x == 0) return x;
    }
    return x * pow2(exponent);
}
