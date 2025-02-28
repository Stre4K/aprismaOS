// libc/include/assert.h
// Author: Stre4K
// Date: 2025-02-28

#ifndef _ASSERT_H
#define _ASSERT_H

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

#endif  // _ASSERT_H
