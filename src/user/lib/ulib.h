// ulib.h
// Minimal runtime for AprismaOS user programs: system call wrappers and a
// few output helpers. System call numbers and error codes come from the
// kernel's ABI header.

#ifndef ULIB_H
#define ULIB_H

#include <kernel/abi/syscall.h>
#include <stddef.h>

// Raw system call: number in rax, arguments in rdi, rsi, rdx, result in rax
static inline long syscall3(long number, long a, long b, long c)
{
    long result;
    __asm__ volatile("int $0x80"
                     : "=a"(result)
                     : "a"(number), "D"(a), "S"(b), "d"(c)
                     : "memory");
    return result;
}

__attribute__((noreturn)) void exit(int code);
long write(int fd, const void *buf, size_t len);
long read(int fd, void *buf, size_t len);
long open(const char *path);
long close(int fd);
long getpid(void);
long sleep(unsigned ms);
long yield(void);

size_t strlen(const char *s);
void print(const char *s);
void print_int(long value);

#endif
