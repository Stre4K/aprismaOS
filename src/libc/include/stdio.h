#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

int printf(const char *__restrict, ...) __attribute__((format(printf, 1, 2)));
int vprintf(const char *__restrict, va_list);
int snprintf(char *__restrict, size_t, const char *__restrict, ...)
    __attribute__((format(printf, 3, 4)));
int vsnprintf(char *__restrict, size_t, const char *__restrict, va_list);

/* Non-standard: the formatting engine behind the functions above (and the
   kernel's printk). Formats into any output: sink() receives one character
   at a time and returns EOF to abort. Returns the number of characters
   produced, or -1 on error. */
int vformat(int (*sink)(char c, void *ctx), void *ctx, const char *__restrict, va_list);
int putchar(int);
int puts(const char *);
void perror(const char *);

#ifdef __cplusplus
}
#endif

#endif
