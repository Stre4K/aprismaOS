// mv: move or rename a file or directory. Into a directory ("mv a /tmp/d")
// it keeps its name. Both places must be writable (in /tmp); a file can
// replace a file.
#include "ulib.h"

int main(int argc, char **argv)
{
    if (argc != 3) {
        eprint("usage: mv <from> <to or directory>\n");
        return 1;
    }
    const char *from = argv[1];
    char to[256];
    size_t len = strlen(argv[2]);
    if (len >= sizeof(to) - 1) {
        eprint("mv: name too long\n");
        return 1;
    }
    memcpy(to, argv[2], len + 1);

    stat_t st;
    if (stat(to, &st) == 0 && st.type == STAT_DIR) {
        const char *base = from;
        for (const char *p = from; *p; p++)
            if (*p == '/' && p[1])
                base = p + 1;
        size_t base_len = strlen(base);
        while (base_len && base[base_len - 1] == '/')
            base_len--;  // "mv d/ e" keeps the name "d"
        if (len + 1 + base_len >= sizeof(to)) {
            eprint("mv: name too long\n");
            return 1;
        }
        if (len == 0 || to[len - 1] != '/')
            to[len++] = '/';
        memcpy(to + len, base, base_len);
        to[len + base_len] = '\0';
    }

    long r = rename(from, to);
    if (r < 0) {
        eprint("mv: ");
        eprint(from);
        eprint(r == -ENOENT    ? ": no such file or directory\n"
               : r == -EROFS   ? ": read-only (only /tmp is writable)\n"
               : r == -EEXIST  ? ": the target exists (only a file can replace a file)\n"
               : r == -EINVAL  ? ": cannot move a directory into itself\n"
                               : ": cannot move\n");
        return 1;
    }
    return 0;
}
