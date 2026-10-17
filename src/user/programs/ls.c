// ls: list directories (the current directory without arguments). For a file
// it prints the name and size, like the kernel shell's ls.
#include "ulib.h"

static void print_entry(const char *name, const dirent_t *entry)
{
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
    dirent_t entry = { .size = st.size, .is_dir = st.type == STAT_DIR };
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
    if (argc < 2)
        return list(".", 0);

    int failed = 0;
    for (int i = 1; i < argc; i++)
        failed |= list(argv[i], argc > 2);
    return failed;
}
