// sqrt.c
// <math.h>
// Author: Stre4K
// Date: 2025-02-25

#include <math.h>

double sqrt(double x) {
    if (x < 0) {
        return -1; // ERROR sqrt of negative number
    }

    double guess = x;
    double epsilon = 0.00001; // Precision

    while ((guess * guess - x > epsilon) || (x - guess * guess > epsilon)) {
        guess = (guess + x / guess) / 2;
    }

    return guess;
}
