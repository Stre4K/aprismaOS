#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <errno.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

__attribute__((__noreturn__)) void abort(void);

#ifdef __cplusplus
}
#endif

#endif
