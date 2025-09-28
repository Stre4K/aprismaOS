// fmod.c
// <math.h>
// Author: Stre4K
// Date: 2025-03-09

#include <math.h>

double fmod(double x, double y) {
    return x - y * floor(x / y);
}
