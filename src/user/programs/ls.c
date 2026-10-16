// ls: list directories (the root directory without arguments). For a file
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

// Find a file by listing its parent directory (there is no stat call)
static int show_file(const char *path)
{
    char parent[DIRENT_NAME_MAX];
    size_t slash = 0;
    size_t len = strlen(path);
    if (len >= sizeof(parent))
        return 1;
    for (size_t i = 0; i < len; i++) {
        parent[i] = path[i];
        if (path[i] == '/')
            slash = i;
    }
    parent[slash > 0 ? slash : 1] = '\0';  // "/README" -> "/"
    const char *name = path + slash + 1;

    dirent_t entry;
    for (size_t i = 0; readdir(parent, i, &entry) > 0; i++) {
        if (strcmp(entry.name, name) == 0) {
            print_entry(path, &entry);
            return 0;
        }
    }
    return 1;
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
        print("ls: ");
        print(dir);
        print(": no such file or directory\n");
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
        return list("/", 0);

    int failed = 0;
    for (int i = 1; i < argc; i++)
        failed |= list(argv[i], argc > 2);
    return failed;
}
