// strrev.c
// <string.h>
// Author: Stre4K
// Date: 2025-02-23

#include <string.h>

void strrev(char* str) {
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = temp;
    }
}
