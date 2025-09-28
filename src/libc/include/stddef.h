#ifndef _STDDEF_H
#define _STDDEF_H

/*
 * Minimal <stddef.h> for a freestanding kernel
 * Provides basic types and macros that the compiler expects.
 */

/* Define NULL */
#ifndef NULL
#define NULL ((void *)0)
#endif

/* Define size_t (unsigned integer type used for sizes) */
#ifndef __SIZE_TYPE__
typedef unsigned long size_t;
#else
typedef __SIZE_TYPE__ size_t;
#endif

/* Define ptrdiff_t (signed type for pointer subtraction) */
#ifndef __PTRDIFF_TYPE__
typedef long ptrdiff_t;
#else
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#endif

/* Define offsetof (offset of a struct member) */
#define offsetof(type, member) ((size_t)&(((type *)0)->member))

#endif /* _STDDEF_H */
