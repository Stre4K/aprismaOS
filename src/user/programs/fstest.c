// fstest: checks the writable filesystem through the system calls (run by
// the kernel's `test user`). Everything it makes in /tmp is removed again.
// Exits with the number of failed checks.
#include "ulib.h"

#define KERNEL_ADDRESS 0xFFFFFFFF80100000UL
#define FILE_MAX       (4L << 20)  // the kernel's FS_FILE_MAX

int main(void);

static int failures;

static void check(int ok, const char *what)
{
    if (!ok) {
        print("fstest: failed: ");
        print(what);
        print("\n");
        failures++;
    }
}

static int same(const char *a, const char *b, long n)
{
    for (long i = 0; i < n; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

static long write_all(int fd, const char *s)
{
    return write(fd, s, strlen(s));
}

static void check_files(void)
{
    char buf[64];
    long fd = open("/tmp/a", O_WRONLY | O_CREAT);
    check(fd >= 3, "create a file");
    check(write_all((int)fd, "hello") == 5 && write_all((int)fd, " world") == 6, "write to it");
    check(read((int)fd, buf, 1) == -EBADF, "a write-only file cannot be read");
    close((int)fd);

    fd = open("/tmp/a", O_RDONLY);
    check(read((int)fd, buf, sizeof(buf)) == 11 && same(buf, "hello world", 11), "read it back");
    check(read((int)fd, buf, 1) == 0, "then the end");
    check(write((int)fd, "x", 1) == -EBADF, "a read-only file cannot be written");
    close((int)fd);

    // O_RDWR: reads and writes share one position
    fd = open("/tmp/a", O_RDWR);
    check(read((int)fd, buf, 6) == 6, "read the start");
    check(write_all((int)fd, "WORLD") == 5, "overwrite after it");
    close((int)fd);
    fd = open("/tmp/a", O_RDONLY);
    check(read((int)fd, buf, sizeof(buf)) == 11 && same(buf, "hello WORLD", 11), "overwritten in place");
    close((int)fd);

    // O_APPEND and O_TRUNC
    fd = open("/tmp/a", O_WRONLY | O_APPEND);
    check(write_all((int)fd, "!") == 1, "append");
    close((int)fd);
    stat_t st;
    check(stat("/tmp/a", &st) == 0 && st.size == 12 && st.type == STAT_FILE && st.writable, "stat a file");
    fd = open("/tmp/a", O_WRONLY | O_TRUNC);
    check(fstat((int)fd, &st) == 0 && st.size == 0, "O_TRUNC empties it");
    close((int)fd);

    // Two opens have their own positions
    fd = open("/tmp/a", O_RDWR | O_TRUNC);
    write_all((int)fd, "abcdef");
    long a = open("/tmp/a", O_RDONLY), b = open("/tmp/a", O_RDONLY);
    char ca, cb;
    read((int)a, &ca, 1);
    read((int)a, &ca, 1);
    read((int)b, &cb, 1);
    check(ca == 'b' && cb == 'a', "separate opens, separate positions");
    close((int)a);
    close((int)b);
    close((int)fd);

    // Removed while open: still readable, and the name is free again
    fd = open("/tmp/a", O_RDONLY);
    check(unlink("/tmp/a") == 0, "remove an open file");
    check(stat("/tmp/a", &st) == -ENOENT, "its name is gone");
    check(read((int)fd, buf, sizeof(buf)) == 6 && same(buf, "abcdef", 6), "but it can still be read");
    long again = open("/tmp/a", O_WRONLY | O_CREAT);
    check(again >= 0, "a new file with the same name");
    close((int)again);
    close((int)fd);
    unlink("/tmp/a");
}

static void check_errors(void)
{
    stat_t st;
    check(open("/etc/motd", O_WRONLY) == -EROFS, "the ramdisk is read-only");
    check(open("/etc/new", O_WRONLY | O_CREAT) == -EROFS, "no new files in it");
    check(open("/tmp/none", O_RDONLY) == -ENOENT, "open a missing file");
    check(open("/tmp/none/x", O_WRONLY | O_CREAT) == -ENOENT, "create in a missing directory");
    check(open("/tmp", O_RDONLY) == -EISDIR, "open a directory");
    check(open("/tmp/x", O_ACCMODE) == -EINVAL, "bad access mode");
    check(open("/tmp/x", O_RDONLY | 0x100000) == -EINVAL, "unknown flags");
    check(open((const char *)KERNEL_ADDRESS, O_RDONLY) == -EFAULT, "path in kernel memory");

    check(mkdir("/tmp/d") == 0, "make a directory");
    check(mkdir("/tmp/d") == -EEXIST, "it exists");
    check(mkdir("/etc/d") == -EROFS, "not in the ramdisk");
    check(mkdir("/tmp/none/d") == -ENOENT, "not in a missing directory");
    long fd = open("/tmp/d/f", O_WRONLY | O_CREAT);
    close((int)fd);
    check(rmdir("/tmp/d") == -ENOTEMPTY, "remove a full directory");
    check(unlink("/tmp/d") == -EISDIR, "unlink a directory");
    check(rmdir("/tmp/d/f") == -ENOTDIR, "rmdir a file");
    check(unlink("/etc/motd") == -EROFS, "unlink a ramdisk file");
    check(rmdir("/tmp") == -EROFS, "remove /tmp itself");
    check(unlink("/tmp/d/f") == 0 && rmdir("/tmp/d") == 0, "remove them in order");
    check(unlink("/tmp/d") == -ENOENT, "and they are gone");

    check(stat("/etc/motd", &st) == 0 && st.type == STAT_FILE && st.size > 0 && !st.writable, "stat a ramdisk file");
    check(stat("/tmp", &st) == 0 && st.type == STAT_DIR && st.writable, "stat /tmp");
    check(stat("/", &st) == 0 && st.type == STAT_DIR && !st.writable, "stat the root");
    check(stat("/tmp", (stat_t *)KERNEL_ADDRESS) == -EFAULT, "stat into kernel memory");
    check(stat("/tmp", (stat_t *)main) == -EFAULT, "stat into read-only code");
    check(fstat(9, &st) == -EBADF, "fstat a closed descriptor");
}

static void check_descriptors(void)
{
    stat_t st;
    check(isatty(0) && isatty(1), "stdin and stdout are the console");
    int ends[2];
    pipe(ends);
    check(fstat(ends[0], &st) == 0 && st.type == STAT_PIPE && !isatty(ends[0]), "a pipe is not the console");
    close(ends[0]);
    close(ends[1]);

    // dup2: send stdout to a file for a while
    long saved = dup2(1, 9);
    long file = open("/tmp/out", O_WRONLY | O_CREAT | O_TRUNC);
    check(saved == 9 && dup2((int)file, 1) == 1, "dup2 onto stdout");
    print("into the file");
    check(dup2((int)saved, 1) == 1, "stdout back");
    close(9);
    check(fstat((int)file, &st) == 0 && st.size == 13, "print went into the file");
    write_all((int)file, "!");  // the same open file: dup2 shared the position
    close((int)file);
    check(stat("/tmp/out", &st) == 0 && st.size == 14, "descriptors from dup2 share the position");
    unlink("/tmp/out");

    check(dup2(9, 3) == -EBADF, "dup2 from a closed descriptor");
    check(dup2(0, 16) == -EBADF, "dup2 past the table");
    check(dup2(0, 0) == 0, "dup2 onto itself");
}

static void check_listing_and_programs(void)
{
    // A program copied into /tmp runs from there
    long in = open("/bin/exitcode", O_RDONLY);
    long out = open("/tmp/ec", O_WRONLY | O_CREAT);
    char buf[4096];
    long n;
    while ((n = read((int)in, buf, sizeof(buf))) > 0)
        write((int)out, buf, (size_t)n);
    close((int)in);
    close((int)out);
    int code = 0;
    long pid = spawn("/tmp/ec", 0, 0);
    check(pid > 0 && wait((int)pid, &code) == 0 && code == 42, "run a program from /tmp");

    dirent_t entry;
    int found = 0;
    for (size_t i = 0; readdir("/tmp", i, &entry) > 0; i++)
        found |= strcmp(entry.name, "ec") == 0 && !entry.is_dir && entry.size > 0;
    check(found, "readdir lists the copy");
    unlink("/tmp/ec");
    check(readdir("/tmp", 0, &entry) == 0, "/tmp is empty again");
}

// Fill /tmp: files stop at 4 MiB (-EFBIG), /tmp as a whole at 16 MiB (-ENOSPC)
static void check_limits(void)
{
    static char chunk[65536];
    char name[] = "/tmp/fill0";
    long r = 0, total = 0;
    int files = 0;
    for (; files < 9 && r != -ENOSPC; files++) {
        name[9] = (char)('0' + files);
        long fd = open(name, O_WRONLY | O_CREAT);
        long size = 0;
        while ((r = write((int)fd, chunk, sizeof(chunk))) > 0)
            size += r;
        check(size <= FILE_MAX && (r == -ENOSPC || (r == -EFBIG && size == FILE_MAX)), "a file stops at its limit");
        total += size;
        close((int)fd);
    }
    check(r == -ENOSPC && total > (15L << 20), "/tmp fills up at about 16 MiB");
    for (int i = 0; i < files; i++) {
        name[9] = (char)('0' + i);
        unlink(name);
    }
    long fd = open("/tmp/after", O_WRONLY | O_CREAT);
    check(write((int)fd, chunk, sizeof(chunk)) == sizeof(chunk), "removing files gives the space back");
    close((int)fd);
    unlink("/tmp/after");
}

int main(void)
{
    check_files();
    check_errors();
    check_descriptors();
    check_listing_and_programs();
    check_limits();

    print(failures ? "fstest: FAILED\n" : "fstest: ok\n");
    return failures;
}
