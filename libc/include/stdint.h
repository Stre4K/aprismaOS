#ifndef _STDINT_H
#define _STDINT_H

/*
 * Minimal <stdint.h> for a freestanding kernel.
 * Provides fixed-width integer types and standard typedefs.
 */

/* Exact-width integer types */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

/* Minimum-width integer types */
typedef int8_t int_least8_t;
typedef uint8_t uint_least8_t;
typedef int16_t int_least16_t;
typedef uint16_t uint_least16_t;
typedef int32_t int_least32_t;
typedef uint32_t uint_least32_t;
typedef int64_t int_least64_t;
typedef uint64_t uint_least64_t;

/* Fastest minimum-width integer types */
typedef int32_t int_fast8_t;
typedef uint32_t uint_fast8_t;
typedef int32_t int_fast16_t;
typedef uint32_t uint_fast16_t;
typedef int32_t int_fast32_t;
typedef uint32_t uint_fast32_t;
typedef int64_t int_fast64_t;
typedef uint64_t uint_fast64_t;

/* Pointer-sized integer types */
typedef unsigned long uintptr_t;
typedef long intptr_t;

/* Greatest-width integer types */
typedef long long intmax_t;
typedef unsigned long long uintmax_t;

/* Limits for standard types (optional, but handy) */
#define INT8_MIN (-128)
#define INT8_MAX 127
#define UINT8_MAX 0xFF

#define INT16_MIN (-32768)
#define INT16_MAX 32767
#define UINT16_MAX 0xFFFF

#define INT32_MIN (-2147483647 - 1)
#define INT32_MAX 2147483647
#define UINT32_MAX 0xFFFFFFFFU

#define INT64_MIN (-9223372036854775807LL - 1)
#define INT64_MAX 9223372036854775807LL
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL

#endif /* _STDINT_H */
