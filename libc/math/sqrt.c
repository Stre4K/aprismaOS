// sqrt.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-11

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

double cbrt(double x) {
    if (x < 0) {
        return -1; // ERROR cbrt of negative number
    }

    double guess = x;
    double epsilon = 0.00001; // Precision

    while ((guess * guess * guess - x > epsilon) || (x - guess * guess * guess > epsilon)) {
        guess = (guess + x / (guess * guess)) / 2;
    }
}
