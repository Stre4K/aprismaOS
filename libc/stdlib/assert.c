// assert.c
#include "assert.h"

void __assert_fail(const char *expr, const char *file, int line,
                   const char *func) {
    printf("Assertion failed: %s, file %s, line %d, function %s\n", expr, file,
           line, func);
    abort();  // Halt the system or trigger a kernel panic
}
