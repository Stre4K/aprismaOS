#include <string.h>

char *strcpy(char *__restrict dest, const char *__restrict src) {
    char *original = dest;
    while ((*dest++ = *src++));
    return original;
}
