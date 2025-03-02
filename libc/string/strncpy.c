// strncpy.c
// <string.h>
// Author: Stre4K
// Date: 2025-03-02

#include <string.h>

char *strncpy(char *__restrict dest, const char *__restrict src, size_t n) {
    size_t i;

    // Copy characters until `n` is reached or `src` ends
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    // Pad the rest of `dest` with null characters if `src` is shorter than `n`
    for (; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}
