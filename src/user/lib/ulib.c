// ulib.c

#include "ulib.h"

void exit(int code)
{
    syscall3(SYS_EXIT, code, 0, 0);
    for (;;)
        ;  // not reached
}

long write(int fd, const void *buf, size_t len)
{
    return syscall3(SYS_WRITE, fd, (long)buf, (long)len);
}

long read(int fd, void *buf, size_t len)
{
    return syscall3(SYS_READ, fd, (long)buf, (long)len);
}

long open(const char *path, int flags)
{
    return syscall3(SYS_OPEN, (long)path, flags, 0);
}

long close(int fd)
{
    return syscall3(SYS_CLOSE, fd, 0, 0);
}

long getpid(void)
{
    return syscall3(SYS_GETPID, 0, 0, 0);
}

long sleep(unsigned ms)
{
    return syscall3(SYS_SLEEP, ms, 0, 0);
}

long yield(void)
{
    return syscall3(SYS_YIELD, 0, 0, 0);
}

long spawn(const char *path, char *const argv[], const spawn_options_t *options)
{
    return syscall3(SYS_SPAWN, (long)path, (long)argv, (long)options);
}

long pipe(int fds[2])
{
    return syscall3(SYS_PIPE, (long)fds, 0, 0);
}

long dup2(int fd, int new_fd)
{
    return syscall3(SYS_DUP2, fd, new_fd, 0);
}

long mkdir(const char *path)
{
    return syscall3(SYS_MKDIR, (long)path, 0, 0);
}

long unlink(const char *path)
{
    return syscall3(SYS_UNLINK, (long)path, 0, 0);
}

long rmdir(const char *path)
{
    return syscall3(SYS_RMDIR, (long)path, 0, 0);
}

long stat(const char *path, stat_t *st)
{
    return syscall3(SYS_STAT, (long)path, (long)st, 0);
}

long fstat(int fd, stat_t *st)
{
    return syscall3(SYS_FSTAT, fd, (long)st, 0);
}

long lseek(int fd, long offset, int whence)
{
    return syscall3(SYS_LSEEK, fd, offset, whence);
}

long rename(const char *from, const char *to)
{
    return syscall3(SYS_RENAME, (long)from, (long)to, 0);
}

long chdir(const char *path)
{
    return syscall3(SYS_CHDIR, (long)path, 0, 0);
}

long getcwd(char *buf, size_t size)
{
    return syscall3(SYS_GETCWD, (long)buf, (long)size, 0);
}

int isatty(int fd)
{
    stat_t st;
    return fstat(fd, &st) == 0 && st.type == STAT_CONSOLE;
}

long wait(int pid, int *code)
{
    return syscall3(SYS_WAIT, pid, (long)code, 0);
}

long waitpid(int pid, int *code, int flags)
{
    return syscall3(SYS_WAIT, pid, (long)code, flags);
}

long kill(int pid)
{
    return syscall3(SYS_KILL, pid, 0, 0);
}

long readdir(const char *dir, size_t index, dirent_t *entry)
{
    return syscall3(SYS_READDIR, (long)dir, (long)index, (long)entry);
}

long brk(void *end)
{
    return syscall3(SYS_BRK, (long)end, 0, 0);
}

void *sbrk(long increment)
{
    long current = brk(0);
    if (increment == 0)
        return (void *)current;
    if (brk((void *)(current + increment)) < 0)
        return (void *)-1;
    return (void *)current;
}

void *memset(void *dst, int value, size_t n)
{
    unsigned char *d = dst;
    while (n--)
        *d++ = (unsigned char)value;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

void print(const char *s)
{
    write(1, s, strlen(s));
}

void eprint(const char *s)
{
    write(2, s, strlen(s));
}

static void write_int(int fd, long value)
{
    char buf[24];
    int i = sizeof(buf);
    unsigned long magnitude = value < 0 ? 0UL - (unsigned long)value : (unsigned long)value;

    do {
        buf[--i] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude);
    if (value < 0)
        buf[--i] = '-';

    write(fd, buf + i, sizeof(buf) - i);
}

void print_int(long value)
{
    write_int(1, value);
}

void eprint_int(long value)
{
    write_int(2, value);
}
