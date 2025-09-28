// pow.c
// <math.h>
// Author: Stre4K
// Date: 2025-02-23

#include <math.h>

// Helper function for integer exponents
double pow_int(double base, int exponent){
    double result = 1.0;
    int exp = (exponent < 0) ? -exponent : exponent;

    while (exp > 0) {
        if (exp % 2 == 1) {
            result *= base;
        }
        base *= base;
        exp /= 2;
    }

    return (exponent < 0) ? 1.0 / result : result;
}


// Main pow function
double pow(double base, double exponent) {
    if (exponent == (int)exponent) {
        return pow_int(base, (int)exponent);
    }

    // For non-integer exponents
    return exp(base * log(exponent));
}
