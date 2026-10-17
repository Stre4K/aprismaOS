// ulib.h
// Minimal runtime for AprismaOS user programs: system call wrappers, memory
// allocation and a few string and output helpers. System call numbers and
// error codes come from the kernel's ABI header.

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
long open(const char *path, int flags);  // O_RDONLY, O_WRONLY | O_CREAT | O_TRUNC, ...
long close(int fd);
long getpid(void);
long sleep(unsigned ms);
long yield(void);
long spawn(const char *path, char *const argv[], const spawn_options_t *options);
long pipe(int fds[2]);
long dup2(int fd, int new_fd);
long mkdir(const char *path);
long unlink(const char *path);
long rmdir(const char *path);
long stat(const char *path, stat_t *st);
long fstat(int fd, stat_t *st);
int isatty(int fd);  // 1 if fd is the console (keyboard and screen)
long lseek(int fd, long offset, int whence);  // SEEK_SET, SEEK_CUR, SEEK_END
long rename(const char *from, const char *to);
long chdir(const char *path);
long getcwd(char *buf, size_t size);
long wait(int pid, int *code);
long readdir(const char *dir, size_t index, dirent_t *entry);
long brk(void *end);           // new end of the heap, or -ENOMEM
void *sbrk(long increment);    // old end of the heap, or (void *)-1

// Heap memory (malloc.c). Blocks are 16-byte aligned; a bad or twice
// freed pointer ends the program with exit code 127.
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);

typedef struct {
    size_t used_bytes, free_bytes;   // payload bytes in used and free blocks
    size_t used_blocks, free_blocks;
} malloc_stats_t;
void malloc_stats(malloc_stats_t *stats);
int malloc_check(void);        // 0 if the heap's bookkeeping is consistent

void *memset(void *dst, int value, size_t n);
void *memcpy(void *dst, const void *src, size_t n);

size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
void print(const char *s);
void eprint(const char *s);  // to stderr: error messages
void print_int(long value);
void eprint_int(long value);

#endif
