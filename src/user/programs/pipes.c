// pipes: checks pipes, descriptors and spawn options (run by the kernel's
// `test user`). Exits with the number of failed checks.
//
//   pipes                  all checks
//   pipes write N          write N bytes of a pattern to stdout
//   pipes read N           read stdin to its end; exit 0 if it was the pattern, N bytes long
//   pipes nostdout         exit 0 if stdout is not open
//   pipes block            wait forever writing to a full pipe only we could read
#include "ulib.h"

#define KERNEL_ADDRESS 0xFFFFFFFF80100000UL
#define MAX_FDS        16  // the kernel's PROCESS_MAX_FDS
#define BIG            100000

int main(int argc, char **argv);

static int failures;

static void check(int ok, const char *what)
{
    if (!ok) {
        print("pipes: failed: ");
        print(what);
        print("\n");
        failures++;
    }
}

static char pattern(long i)
{
    return (char)('a' + i % 23);
}

static long to_long(const char *s)
{
    long value = 0;
    while (*s >= '0' && *s <= '9')
        value = value * 10 + (*s++ - '0');
    return value;
}

static int write_pattern(long total)
{
    char buf[1000];
    for (long done = 0; done < total;) {
        long n = total - done < (long)sizeof(buf) ? total - done : (long)sizeof(buf);
        for (long i = 0; i < n; i++)
            buf[i] = pattern(done + i);
        if (write(1, buf, (size_t)n) != n)
            return 1;
        done += n;
    }
    return 0;
}

static int read_pattern(long fd, long expected)
{
    char buf[700];  // not a divisor of the pipe size, so reads wrap around
    long total = 0, n;
    int ok = 1;
    while ((n = read((int)fd, buf, sizeof(buf))) > 0) {
        for (long i = 0; i < n; i++)
            ok &= buf[i] == pattern(total + i);
        total += n;
    }
    return ok && n == 0 && total == expected;
}

static long start(char *mode, char *count, int in, int out, int pgid)
{
    char *args[] = { "pipes", mode, count, 0 };
    spawn_options_t options = { .fd = { in, out, 2 }, .pgid = pgid };
    return spawn("pipes", args, &options);
}

static int finish(long pid)
{
    int code = -1;
    long how = wait((int)pid, &code);
    return how == 0 ? code : -1;
}

static void check_basics(void)
{
    int fds[2];
    char buf[16];
    check(pipe(fds) == 0 && fds[0] == 3 && fds[1] == 4, "pipe takes the lowest free descriptors");
    check(write(fds[1], "hello", 5) == 5, "write to a pipe");
    check(read(fds[0], buf, 3) == 3 && buf[0] == 'h' && buf[2] == 'l', "read part of it");
    check(read(fds[0], buf, sizeof(buf)) == 2 && buf[0] == 'l' && buf[1] == 'o', "then the rest");
    check(read(fds[1], buf, 1) == -EBADF, "reading the write end fails");
    check(write(fds[0], "x", 1) == -EBADF, "writing the read end fails");
    check(close(fds[1]) == 0, "close the write end");
    check(read(fds[0], buf, sizeof(buf)) == 0, "end of input without writers");
    check(close(fds[0]) == 0, "close the read end");

    check(pipe(fds) == 0, "a second pipe");
    close(fds[0]);
    check(write(fds[1], "x", 1) == -EPIPE, "writing without readers fails");
    close(fds[1]);

    // Filling a pipe: the write that does not fit waits, so give the
    // other end to a child that reads it all
    check(pipe(fds) == 0, "a third pipe");
    long reader = start("read", "4096", fds[0], 1, 0);
    close(fds[0]);
    static char full[4096];
    for (int i = 0; i < 4096; i++)
        full[i] = pattern(i);
    check(write(fds[1], full, sizeof(full)) == 4096, "write exactly a pipe's worth");
    close(fds[1]);
    check(finish(reader) == 0, "a child read it all");

    long fd = open("/etc/motd", O_RDONLY);
    check(write((int)fd, "x", 1) == -EBADF, "ramdisk files cannot be written");
    close((int)fd);
}

static void check_errors(void)
{
    check(pipe((int *)KERNEL_ADDRESS) == -EFAULT, "pipe into kernel memory");
    check(pipe((int *)main) == -EFAULT, "pipe into read-only code");
    check(close(-1) == -EBADF && close(MAX_FDS) == -EBADF && close(9) == -EBADF, "close bad descriptors");

    // A full table: pipe needs two free descriptors and takes none if it fails
    long fds[MAX_FDS];
    int opened = 0;
    while (opened < MAX_FDS && (fds[opened] = open("/etc/motd", O_RDONLY)) >= 0)
        opened++;
    int pair[2];
    check(opened == MAX_FDS - 3, "13 descriptors next to 0, 1 and 2");
    check(pipe(pair) == -EMFILE, "pipe in a full table");
    close((int)fds[--opened]);
    check(pipe(pair) == -EMFILE, "pipe with one free descriptor");
    check(open("/etc/motd", O_RDONLY) == fds[opened], "and the free one is still free");
    while (opened >= 0)
        close((int)fds[opened--]);

    // Spawn options
    spawn_options_t options = { .fd = { 0, 9, 2 }, .pgid = 0 };
    check(spawn("hello", 0, &options) == -EBADF, "spawn with a closed descriptor");
    options.fd[1] = MAX_FDS;
    check(spawn("hello", 0, &options) == -EBADF, "spawn with a descriptor out of range");
    options.fd[1] = 1;
    options.pgid = 1;
    check(spawn("hello", 0, &options) == -EPERM, "joining a group that is not ours");
    check(spawn("hello", 0, (const spawn_options_t *)KERNEL_ADDRESS) == -EFAULT, "options in kernel memory");

    long leader = start("write", "0", 0, 1, 0);
    long member = start("write", "0", 0, 1, (int)leader);
    check(leader > 0 && member > 0, "a child joins its sibling's group");
    finish(member);
    finish(leader);
    long same_group = start("write", "0", 0, 1, (int)getpid());  // we lead a group of our own
    check(same_group > 0, "a child joins our group");
    if (same_group > 0)
        finish(same_group);

    long closed = start("nostdout", "0", 0, -1, 0);
    check(finish(closed) == 0, "a child without stdout");
}

static void check_transfers(void)
{
    char count[] = "100000";
    int fds[2];

    // Child writes, we read
    pipe(fds);
    long writer = start("write", count, 0, fds[1], 0);
    close(fds[1]);
    check(read_pattern(fds[0], BIG), "100000 bytes from a child");
    close(fds[0]);
    check(finish(writer) == 0, "the writer finished");

    // We write, child reads
    pipe(fds);
    long reader = start("read", count, fds[0], 1, 0);
    close(fds[0]);
    static char buf[BIG];
    for (long i = 0; i < BIG; i++)
        buf[i] = pattern(i);
    long first = write(fds[1], buf, BIG);  // one write moves at most 64 KiB
    check(first == 65536, "a big write goes through up to 64 KiB");
    check(first > 0 && write(fds[1], buf + first, (size_t)(BIG - first)) == BIG - first,
          "the rest in a second write");
    close(fds[1]);
    check(finish(reader) == 0, "the reader got them all");

    // write | cat | read, all children
    int a[2], b[2];
    pipe(a);
    pipe(b);
    writer = start("write", count, 0, a[1], 0);
    char *cat_args[] = { "cat", 0 };
    spawn_options_t middle = { .fd = { a[0], b[1], 2 }, .pgid = (int)writer };
    long cat = spawn("cat", cat_args, &middle);
    reader = start("read", count, b[0], 1, (int)writer);
    close(a[0]);
    close(a[1]);
    close(b[0]);
    close(b[1]);
    check(finish(writer) == 0 && finish(cat) == 0 && finish(reader) == 0, "through cat in a pipeline");
}

int main(int argc, char **argv)
{
    if (argc > 2 && strcmp(argv[1], "write") == 0)
        return write_pattern(to_long(argv[2]));
    if (argc > 2 && strcmp(argv[1], "read") == 0)
        return read_pattern(0, to_long(argv[2])) ? 0 : 1;
    if (argc > 1 && strcmp(argv[1], "nostdout") == 0)
        return write(1, "x", 1) == -EBADF ? 0 : 1;
    if (argc > 1 && strcmp(argv[1], "block") == 0) {
        int fds[2];
        static char more[4097];
        pipe(fds);
        write(fds[1], more, sizeof(more));  // the last byte never fits
        return 1;                           // Ctrl+C must end us before this
    }

    check_basics();
    check_errors();
    check_transfers();

    // stdin, stdout and stderr are ordinary descriptors
    check(close(0) == 0, "close stdin");
    check(open("/etc/motd", O_RDONLY) == 0, "open takes descriptor 0 again");
    char first;
    check(read(0, &first, 1) == 1 && first == 'W', "and reads the file through it");

    print(failures ? "pipes: FAILED\n" : "pipes: ok\n");
    return failures;
}
