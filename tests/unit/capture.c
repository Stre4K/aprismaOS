// capture.c
// Stands in for AprismaOS's putchar: everything the code under test prints
// is collected in a buffer that tests can inspect.

#include "aprisma.h"

#include <string.h>

static char buffer[4096];
static size_t length;

int aprisma_putchar(int c)
{
    if (length < sizeof(buffer) - 1)
        buffer[length++] = (char)c;
    buffer[length] = '\0';
    return (unsigned char)c;
}

void capture_reset(void)
{
    length = 0;
    buffer[0] = '\0';
}

const char *capture_text(void)
{
    return buffer;
}

size_t capture_length(void)
{
    return length;
}
