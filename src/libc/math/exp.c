// exp.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-06

#include <math.h>

// Compute e^x for small values using Taylor series
double exp_taylor(double x) {
    double sum = 1.0;
    double term = 1.0;
    int n = 1;

    while (term > 1e-10 || term < -1e-10) {  // Stop when term is too small
        term *= x / n;
        sum += term;
        n++;
    }

    return sum;
}

// Compute e^x for any x
double exp(double x) {
    if (x < 0) return 1.0 / exp(-x);  // Handle negative x

    int int_part = (int)x;   // Integer part
    double frac_part = x - int_part;  // Fractional part

    // Compute e^int_part using fast exponentiation
    double int_exp = 1.0;
    const double e = 2.718281828459045;
    for (int i = 0; i < int_part; i++) {
        int_exp *= e;
    }

    // Compute e^frac_part using Taylor series
    double frac_exp = exp_taylor(frac_part);

    return int_exp * frac_exp;  // Combine both parts
}
