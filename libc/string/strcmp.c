// strcmp.c
// <string.h>
// Author: Stre4K
// Date: 2025-02-21

#include <string.h>

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {  // Continue while characters are equal and not '\0'
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;  // Return ASCII difference
}
