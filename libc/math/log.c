// log.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-06

#include <math.h>

// Natural logarithm approximation using Newton's method
// log(x) = ln(x)
double log(double x) {
    if (x <= 0) return 0;  // log is undefined for x <= 0

    double result = x - 1.0;  // Initial guess
    double epsilon = 1e-10;

    for (int i = 0; i < 100; i++) {  // Limit iterations
        double e_pow = exp(result);
        double diff = (e_pow - x) / e_pow;
        if (diff > -epsilon && diff < epsilon) break;
        result -= diff;
    }

    return result;
}
