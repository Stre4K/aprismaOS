// ls: list directories (the current directory without arguments). For a file
// it prints the name and size, like the kernel shell's ls. With -l each
// line starts with the time of the last change (UTC) and the size:
//
//   2026-10-17 10:25     5432  hello
//   2026-10-17 10:25           bin/
#include "ulib.h"

static int long_format;  // -l

static void print_long(const char *name, const dirent_t *entry)
{
    char when[20];
    format_time(when, (long)entry->mtime);
    when[16] = '\0';  // to the minute
    print(when);
    char size[12] = "           ";
    if (!entry->is_dir) {
        unsigned long n = entry->size;
        int i = 10;
        do {
            size[i--] = (char)('0' + n % 10);
            n /= 10;
        } while (n && i >= 0);
    }
    print(size);
    print("  ");
    print(name);
    print(entry->is_dir ? "/\n" : "\n");
}

static void print_entry(const char *name, const dirent_t *entry)
{
    if (long_format) {
        print_long(name, entry);
        return;
    }
    print(name);
    if (entry->is_dir) {
        print("/\n");
        return;
    }
    for (size_t len = strlen(name); len < 20; len++)
        print(" ");
    print(" ");
    print_int((long)entry->size);
    print(" bytes\n");
}

static int show_file(const char *path)
{
    stat_t st;
    if (stat(path, &st) < 0)
        return 1;
    dirent_t entry = { .size = st.size, .is_dir = st.type == STAT_DIR, .mtime = st.mtime };
    print_entry(path, &entry);
    return 0;
}

// With several arguments, each directory's listing gets a "name:" header
static int list(const char *dir, int headers)
{
    static int printed;  // arguments shown so far, for the blank lines between
    dirent_t entry;
    long result = readdir(dir, 0, &entry);
    if (result == -ENOTDIR) {
        printed++;
        return show_file(dir);
    }
    if (result < 0) {
        eprint("ls: ");
        eprint(dir);
        eprint(": no such file or directory\n");
        return 1;
    }

    if (headers) {
        print(printed++ ? "\n" : "");
        print(dir);
        print(":\n");
    }

    for (size_t i = 1; result > 0; i++) {
        print_entry(entry.name, &entry);
        result = readdir(dir, i, &entry);
    }
    return 0;
}

int main(int argc, char **argv)
{
    int first = 1;
    if (argc > 1 && strcmp(argv[1], "-l") == 0) {
        long_format = 1;
        first = 2;
    }
    if (first >= argc)
        return list(".", 0);

    int failed = 0;
    for (int i = first; i < argc; i++)
        failed |= list(argv[i], argc - first > 1);
    return failed;
}
