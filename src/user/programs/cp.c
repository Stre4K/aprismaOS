// cp: copy a file. The copy can go into a directory ("cp /bin/hello /tmp")
// or get a new name; it can only be made in a writable place (/tmp).
#include "ulib.h"

static void fail(const char *what, const char *path)
{
    print("cp: ");
    print(path);
    print(what);
    print("\n");
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        print("usage: cp <file> <copy or directory>\n");
        return 1;
    }
    const char *from = argv[1];
    char to[256];
    size_t len = strlen(argv[2]);
    if (len >= sizeof(to) - 1) {
        fail(": name too long", argv[2]);
        return 1;
    }
    memcpy(to, argv[2], len + 1);

    // Into a directory: keep the file's name
    stat_t st;
    if (stat(to, &st) == 0 && st.type == STAT_DIR) {
        const char *base = from;
        for (const char *p = from; *p; p++)
            if (*p == '/')
                base = p + 1;
        if (len + 1 + strlen(base) >= sizeof(to)) {
            fail(": name too long", argv[2]);
            return 1;
        }
        if (len == 0 || to[len - 1] != '/')
            to[len++] = '/';
        memcpy(to + len, base, strlen(base) + 1);
    }

    long in = open(from, O_RDONLY);
    if (in < 0) {
        fail(in == -EISDIR ? ": is a directory" : ": no such file", from);
        return 1;
    }
    long out = open(to, O_WRONLY | O_CREAT | O_TRUNC);
    if (out < 0) {
        fail(out == -EROFS ? ": read-only (only /tmp is writable)" : ": cannot create", to);
        return 1;
    }

    char buf[4096];
    long n;
    while ((n = read((int)in, buf, sizeof(buf))) > 0) {
        if (write((int)out, buf, (size_t)n) != n) {
            fail(": no space left", to);
            return 1;
        }
    }
    return n < 0;
}
