// math.h
// Author: Stre4K
// Date: 2025-03-14

#ifndef _MATH_H
#define _MATH_H

#define HUGE_VAL    (__builtin_huge_val())
#define INFINITY    (__builtin_inff())
#define NAN         (__builtin_nanf(""))

#define isnan(x)    __builtin_isnan(x)
#define isinf(x)    __builtin_isinf(x)
#define isfinite(x) __builtin_isfinite(x)

double pow(double base, double exponent);
double exp(double x);
double log(double x);
double log10(double x);
double sqrt(double x);
double cbrt(double x);
int abs(int x);
long labs(long x);
double fabs(double x);
double fmod(double x, double y);
int max(int a, int b);
int min(int a, int b);
double floor(double x);
double ceil(double x);
double round(double x);
double frexp(double x, int *exponent);
double ldexp(double x, int exponent);

#endif  //_MATH_H
