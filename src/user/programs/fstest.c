// fstest: checks the writable filesystem through the system calls (run by
// the kernel's `test user`). Everything it makes in /tmp is removed again.
// Exits with the number of failed checks.
//
//   fstest             all checks
//   fstest cwd DIR     exit 0 if the current directory is DIR
#include "ulib.h"

#define KERNEL_ADDRESS 0xFFFFFFFF80100000UL
#define FILE_MAX       (4L << 20)  // the kernel's FS_FILE_MAX

int main(int argc, char **argv);

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

static void check_current_directory(void)
{
    char cwd[256];
    check(getcwd(cwd, sizeof(cwd)) == 1 && strcmp(cwd, "/") == 0, "we start in /");
    check(getcwd(cwd, 1) == -ERANGE, "getcwd into a small buffer");
    check(getcwd((char *)KERNEL_ADDRESS, 64) == -EFAULT, "getcwd into kernel memory");
    check(chdir("/etc/motd") == -ENOTDIR && chdir("/none") == -ENOENT, "chdir to a file or nowhere");
    check(chdir((const char *)KERNEL_ADDRESS) == -EFAULT, "chdir from kernel memory");

    check(chdir("/tmp") == 0 && mkdir("d") == 0 && chdir("d") == 0, "chdir and a relative mkdir");
    check(getcwd(cwd, sizeof(cwd)) == 6 && strcmp(cwd, "/tmp/d") == 0, "getcwd follows");
    long fd = open("f", O_WRONLY | O_CREAT);
    check(fd >= 0 && write_all((int)fd, "rel") == 3, "a relative open");
    close((int)fd);
    stat_t st;
    check(stat("/tmp/d/f", &st) == 0 && st.size == 3, "it landed in the current directory");
    check(stat("../d/./f", &st) == 0 && stat("./f", &st) == 0, "relative paths with . and ..");
    dirent_t entry;
    check(readdir(".", 0, &entry) == 1 && strcmp(entry.name, "f") == 0, "readdir of .");

    // Children start in our current directory
    char *args[] = { "fstest", "cwd", "/tmp/d", 0 };
    int code = -1;
    long pid = spawn("fstest", args, 0);
    check(pid > 0 && wait((int)pid, &code) == 0 && code == 0, "a child inherits the directory");

    // ... and a program can be started by a relative path
    long in = open("/bin/exitcode", O_RDONLY), out = open("ec", O_WRONLY | O_CREAT);
    char buf[4096];
    long n;
    while ((n = read((int)in, buf, sizeof(buf))) > 0)
        write((int)out, buf, (size_t)n);
    close((int)in);
    close((int)out);
    pid = spawn("./ec", 0, 0);
    check(pid > 0 && wait((int)pid, &code) == 0 && code == 42, "spawn ./program");

    check(chdir("..") == 0 && getcwd(cwd, sizeof(cwd)) == 4 && strcmp(cwd, "/tmp") == 0, "chdir ..");
    unlink("d/ec");
    unlink("d/f");
    rmdir("d");
    chdir("/");
}

static void check_seek(void)
{
    char buf[16];
    long fd = open("/tmp/s", O_RDWR | O_CREAT | O_TRUNC);
    write_all((int)fd, "0123456789");
    check(lseek((int)fd, 0, SEEK_CUR) == 10, "the position after writing");
    check(lseek((int)fd, 2, SEEK_SET) == 2 && read((int)fd, buf, 3) == 3 && same(buf, "234", 3), "SEEK_SET");
    check(lseek((int)fd, 1, SEEK_CUR) == 6 && read((int)fd, buf, 1) == 1 && buf[0] == '6', "SEEK_CUR");
    check(lseek((int)fd, -2, SEEK_END) == 8 && read((int)fd, buf, 5) == 2 && same(buf, "89", 2), "SEEK_END");
    check(lseek((int)fd, 3, SEEK_SET) == 3 && write_all((int)fd, "x") == 1, "write after seeking");
    check(lseek((int)fd, 12, SEEK_SET) == 12 && write_all((int)fd, "!") == 1, "seek past the end");
    lseek((int)fd, 0, SEEK_SET);
    check(read((int)fd, buf, 16) == 13 && same(buf, "012x456789\0\0!", 13), "the gap is zeros");
    check(lseek((int)fd, -1, SEEK_SET) == -EINVAL && lseek((int)fd, 0, 7) == -EINVAL, "bad seeks");
    check(lseek((int)fd, 0, SEEK_CUR) == 13, "a bad seek leaves the position");
    close((int)fd);

    int ends[2];
    pipe(ends);
    check(lseek(ends[0], 0, SEEK_SET) == -ESPIPE && lseek(0, 0, SEEK_CUR) == -ESPIPE, "pipes and the console cannot seek");
    close(ends[0]);
    close(ends[1]);
    check(lseek(9, 0, SEEK_SET) == -EBADF, "seek a closed descriptor");
    unlink("/tmp/s");
}

static void check_rename(void)
{
    char buf[8];
    stat_t st;
    long fd = open("/tmp/r1", O_RDWR | O_CREAT);
    write_all((int)fd, "moved");
    check(rename("/tmp/r1", "/tmp/r2") == 0, "rename a file");
    check(stat("/tmp/r1", &st) == -ENOENT && stat("/tmp/r2", &st) == 0, "under its new name");
    lseek((int)fd, 0, SEEK_SET);
    check(read((int)fd, buf, 5) == 5 && same(buf, "moved", 5), "an open file survives being renamed");
    close((int)fd);

    close((int)open("/tmp/r3", O_WRONLY | O_CREAT));
    check(rename("/tmp/r2", "/tmp/r3") == 0 && stat("/tmp/r3", &st) == 0 && st.size == 5, "a file replaces a file");
    mkdir("/tmp/rd");
    check(rename("/tmp/r3", "/tmp/rd/r") == 0, "into a directory");
    check(rename("/tmp/rd/r", "/tmp/rd") == -EEXIST, "not over a directory");
    check(rename("/tmp/rd", "/tmp/rd/sub") == -EINVAL, "not into itself");
    check(rename("/etc/motd", "/tmp/m") == -EROFS && rename("/tmp/rd/r", "/etc/r") == -EROFS, "not out of or into the ramdisk");
    check(rename("/tmp/none", "/tmp/x") == -ENOENT, "rename something missing");
    check(chdir("/tmp") == 0 && rename("rd", "rd2") == 0 && stat("/tmp/rd2/r", &st) == 0, "relative names, contents move along");
    chdir("/");
    unlink("/tmp/rd2/r");
    rmdir("/tmp/rd2");
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

int main(int argc, char **argv)
{
    if (argc > 2 && strcmp(argv[1], "cwd") == 0) {
        char cwd[256];
        return getcwd(cwd, sizeof(cwd)) >= 0 && strcmp(cwd, argv[2]) == 0 ? 0 : 1;
    }

    check_files();
    check_errors();
    check_descriptors();
    check_listing_and_programs();
    check_current_directory();
    check_seek();
    check_rename();
    check_limits();

    print(failures ? "fstest: FAILED\n" : "fstest: ok\n");
    return failures;
}
