// spawner: checks the spawn, wait and readdir system calls, including how
// they reject bad arguments (run by the kernel's `test user`). Exits with
// the number of failed checks; leaves one child running on purpose, which
// the kernel must clean up after it.
#include "ulib.h"

#define KERNEL_ADDRESS 0xFFFFFFFF80100000UL
#define UNMAPPED       0x500000000UL

int main(void);

static int failures;

static void check(int ok, const char *what)
{
    if (!ok) {
        print("spawner: failed: ");
        print(what);
        print("\n");
        failures++;
    }
}

// Spawn and wait; returns the exit code, or -1000 - error
static int run(const char *path, char *const argv[], long *how)
{
    long pid = spawn(path, argv, 0);
    if (pid < 0)
        return -1000 + (int)pid;
    int code = -999;
    *how = wait((int)pid, &code);
    return code;
}

static void check_spawn(void)
{
    long how;
    char *args[] = { "args", "x", "y", 0 };
    check(run("args", args, &how) == 3 && how == 0, "args x y exits with 3");
    check(run("/bin/exitcode", 0, &how) == 42 && how == 0, "a full path and no argv");
    run("crash", 0, &how);
    check(how == WAIT_KILLED, "wait reports a killed child");

    check(spawn("nope", 0, 0) == -ENOENT, "no such program");
    check(spawn("/bin", 0, 0) == -ENOENT, "a directory is not a program");
    check(spawn("/etc/motd", 0, 0) == -ENOEXEC, "a text file is not a program");
    check(spawn((const char *)KERNEL_ADDRESS, 0, 0) == -EFAULT, "path in kernel memory");
    check(spawn("args", (char *const *)UNMAPPED, 0) == -EFAULT, "argv not mapped");
    char *bad_string[] = { "args", (char *)KERNEL_ADDRESS, 0 };
    check(spawn("args", bad_string, 0) == -EFAULT, "argument in kernel memory");
    char *empty[] = { 0 };
    check(spawn("args", empty, 0) == -EINVAL, "argv without argv[0]");

    static char *many[34];
    for (int i = 0; i < 33; i++)
        many[i] = "x";
    check(spawn("args", many, 0) == -E2BIG, "33 arguments");
    many[32] = 0;
    check(run("args", many, &how) == 32, "32 arguments");

    static char long_text[2100];
    for (int i = 0; i < 2099; i++)
        long_text[i] = 'a';
    char *too_long[] = { "args", long_text, 0 };
    check(spawn("args", too_long, 0) == -E2BIG, "2100 bytes of arguments");
}

static void check_wait(void)
{
    check(wait((int)getpid(), 0) == -ECHILD, "wait for itself");
    check(wait(1, 0) == -ECHILD, "wait for a process that is not a child");

    // Two children at once, waited for in the other order
    char *one[] = { "args", 0 };
    long first = spawn("args", one, 0);
    long second = spawn("exitcode", 0, 0);
    int code_first = 0, code_second = 0;
    check(first > 0 && second > 0 && first != second, "two children");
    check(wait((int)second, &code_second) == 0 && code_second == 42, "wait for the second child");
    check(wait((int)first, &code_first) == 0 && code_first == 1, "then the first");
    check(wait((int)first, 0) == -ECHILD, "a child can be waited for only once");

    // A bad pointer is refused before the child is taken
    long pid = spawn("exitcode", 0, 0);
    check(wait((int)pid, (int *)KERNEL_ADDRESS) == -EFAULT, "exit code into kernel memory");
    check(wait((int)pid, (int *)main) == -EFAULT, "exit code into read-only code");
    check(wait((int)pid, 0) == 0, "the child is still there after refused waits");
}

static void check_readdir(void)
{
    dirent_t entry;
    int found_sh = 0, found_bin = 0;
    for (size_t i = 0; readdir("/bin", i, &entry) > 0; i++)
        found_sh |= strcmp(entry.name, "sh") == 0 && !entry.is_dir && entry.size > 0;
    for (size_t i = 0; readdir("/", i, &entry) > 0; i++)
        found_bin |= strcmp(entry.name, "bin") == 0 && entry.is_dir;
    check(found_sh, "readdir /bin lists sh");
    check(found_bin, "readdir / lists bin/");

    check(readdir("/bin", 100000, &entry) == 0, "past the last entry");
    check(readdir("/nope", 0, &entry) == -ENOENT, "no such directory");
    check(readdir("/etc/motd", 0, &entry) == -ENOTDIR, "a file is not a directory");
    check(readdir("/", 0, (dirent_t *)KERNEL_ADDRESS) == -EFAULT, "entry in kernel memory");
    check(readdir("/", 0, (dirent_t *)main) == -EFAULT, "entry in read-only code");
    check(readdir((const char *)UNMAPPED, 0, &entry) == -EFAULT, "path not mapped");
}

int main(void)
{
    check_spawn();
    check_wait();
    check_readdir();

    // Exit with a child nobody waits for
    check(spawn("exitcode", 0, 0) > 0, "spawn a child left behind");

    print(failures ? "spawner: FAILED\n" : "spawner: ok\n");
    return failures;
}
