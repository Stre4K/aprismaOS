// rmdir: remove empty directories
#include "ulib.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        eprint("usage: rmdir <directory>...\n");
        return 1;
    }
    int failed = 0;
    for (int i = 1; i < argc; i++) {
        long r = rmdir(argv[i]);
        if (r < 0) {
            eprint("rmdir: ");
            eprint(argv[i]);
            eprint(r == -ENOTEMPTY ? ": directory not empty\n"
                  : r == -ENOTDIR ? ": not a directory\n"
                  : r == -EROFS   ? ": read-only (only /tmp is writable)\n"
                                  : ": no such directory\n");
            failed = 1;
        }
    }
    return failed;
}
