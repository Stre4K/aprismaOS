// frexp.c
// <math.h>

#include <math.h>
#include <stdint.h>

// Split x into a mantissa in [0.5, 1) and a power of two: x = m * 2^exp
double frexp(double x, int *exponent) {
    union { double d; uint64_t u; } bits = { x };
    int e = (int)((bits.u >> 52) & 0x7FF);

    if (e == 0) {
        if (x == 0) {  // zero keeps its sign
            *exponent = 0;
            return x;
        }
        // Subnormal: scale into the normal range first
        bits.d = x * 0x1p54;
        e = (int)((bits.u >> 52) & 0x7FF) - 54;
    } else if (e == 0x7FF) {  // inf or nan
        *exponent = 0;
        return x;
    }

    *exponent = e - 1022;
    bits.u = (bits.u & ~(0x7FFULL << 52)) | (1022ULL << 52);
    return bits.d;
}
