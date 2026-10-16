// strncmp.c
// <string.h>

#include <string.h>

int strncmp(const char *s1, const char *s2, size_t n) {
    for (; n > 0; n--, s1++, s2++) {
        if (*s1 != *s2 || *s1 == '\0')
            return *(const unsigned char *)s1 - *(const unsigned char *)s2;
    }
    return 0;
}
