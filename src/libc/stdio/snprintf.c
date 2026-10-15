// snprintf.c
// <stdio.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    char *buffer;
    size_t size;
    size_t length;
} buffer_t;

// Store characters while they fit, leaving room for the terminator; keep
// counting past the end so the caller learns the full length
static int buffer_sink(char c, void *ctx) {
    buffer_t *b = ctx;
    if (b->length + 1 < b->size) {
        b->buffer[b->length] = c;
    }
    b->length++;
    return (unsigned char)c;
}

// Format into buffer, writing at most size - 1 characters plus a NUL.
// Returns the length the full output would have (C99), or -1 on error.
int vsnprintf(char *restrict buffer, size_t size, const char *restrict format, va_list args) {
    buffer_t b = { buffer, size, 0 };
    int result = vformat(buffer_sink, &b, format, args);

    if (size > 0) {
        buffer[b.length < size ? b.length : size - 1] = '\0';
    }
    return result;
}

int snprintf(char *restrict buffer, size_t size, const char *restrict format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}
