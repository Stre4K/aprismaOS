// aprisma.h
// Declarations for the AprismaOS code under test.
//
// The Makefile compiles AprismaOS sources unchanged and then prefixes every
// symbol with "aprisma_" (objcopy --prefix-symbols), so they can be linked
// next to the host C library without clashing. Types here use host types
// with the same size and signedness as AprismaOS's on x86_64.

#ifndef APRISMA_UNDER_TEST_H
#define APRISMA_UNDER_TEST_H

#include <stddef.h>
#include <stdint.h>

/* <string.h> */
void *aprisma_memcpy(void *dst, const void *src, size_t n);
void *aprisma_memmove(void *dst, const void *src, size_t n);
void *aprisma_memset(void *dst, int value, size_t n);
int aprisma_memcmp(const void *a, const void *b, size_t n);
size_t aprisma_strlen(const char *s);
int aprisma_strcmp(const char *a, const char *b);
char *aprisma_strcpy(char *dst, const char *src);
char *aprisma_strncpy(char *dst, const char *src, size_t n);
char *aprisma_strerror(int errnum);
void aprisma_strrev(char *s);

/* <stdio.h> */
int aprisma_printf(const char *format, ...);
int aprisma_puts(const char *s);

/* <stdlib.h> */
void aprisma_qsort(void *base, size_t count, size_t size, int (*comp)(const void *, const void *));

/* <ctype.h> */
int aprisma_isdigit(int c);
int aprisma_isalpha(int c);

/* <math.h> */
int aprisma_abs(int x);
long aprisma_labs(long x);
int aprisma_min(int a, int b);
int aprisma_max(int a, int b);
double aprisma_fabs(double x);
double aprisma_floor(double x);
double aprisma_ceil(double x);
double aprisma_round(double x);
double aprisma_fmod(double x, double y);
double aprisma_sqrt(double x);
double aprisma_cbrt(double x);
double aprisma_pow(double base, double exponent);
double aprisma_exp(double x);
double aprisma_log(double x);
double aprisma_log10(double x);
double aprisma_frexp(double x, int *exponent);
double aprisma_ldexp(double x, int exponent);

/* kernel/core/bitmap.c */
void aprisma_set_bit(uint8_t *bitmap, uint32_t bit);
void aprisma_clear_bit(uint8_t *bitmap, uint32_t bit);
int aprisma_test_bit(uint8_t *bitmap, uint32_t bit);

/* Output written through aprisma_putchar (see capture.c) */
void capture_reset(void);
const char *capture_text(void);
size_t capture_length(void);  // bytes printed, including any NUL bytes

#endif // APRISMA_UNDER_TEST_H
