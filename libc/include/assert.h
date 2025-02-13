// assert.h
#ifndef ASSERT_H
#define ASSERT_H

#include <stdio.h>   // For printf or custom I/O functions
#include <stdlib.h>  // For abort or custom system halt

#ifdef NDEBUG
#define assert(condition) ((void)0)
#else
#define assert(condition)                                            \
    do {                                                             \
        if (!(condition)) {                                          \
            __assert_fail(#condition, __FILE__, __LINE__, __func__); \
        }                                                            \
    } while (0)
#endif

void __assert_fail(const char *expr, const char *file, int line,
                   const char *func);

#endif  // ASSERT_H
