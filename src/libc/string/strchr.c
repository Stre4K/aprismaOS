// strchr.c
// <string.h>

#include <string.h>

// First occurrence of c in s; the terminating NUL counts as part of s
char *strchr(const char *s, int c) {
    for (;; s++) {
        if (*s == (char)c)
            return (char *)s;
        if (*s == '\0')
            return NULL;
    }
}

// Last occurrence of c in s; the terminating NUL counts as part of s
char *strrchr(const char *s, int c) {
    const char *last = NULL;
    for (;; s++) {
        if (*s == (char)c)
            last = s;
        if (*s == '\0')
            return (char *)last;
    }
}
