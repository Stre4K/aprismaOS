// isalpha.c
// <ctype.h>
// Author: Stre4K
// Date: 2025-03-15

#include <ctype.h>

int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
