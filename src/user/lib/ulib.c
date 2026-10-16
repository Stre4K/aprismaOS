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

long open(const char *path)
{
    return syscall3(SYS_OPEN, (long)path, 0, 0);
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

long spawn(const char *path, char *const argv[])
{
    return syscall3(SYS_SPAWN, (long)path, (long)argv, 0);
}

long wait(int pid, int *code)
{
    return syscall3(SYS_WAIT, pid, (long)code, 0);
}

long readdir(const char *dir, size_t index, dirent_t *entry)
{
    return syscall3(SYS_READDIR, (long)dir, (long)index, (long)entry);
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

void print_int(long value)
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

    write(1, buf + i, sizeof(buf) - i);
}
