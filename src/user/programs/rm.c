// rm: remove files; with -d also empty directories
#include "ulib.h"

int main(int argc, char **argv)
{
    int dirs = argc > 1 && strcmp(argv[1], "-d") == 0;
    if (argc < 2 + dirs) {
        print("usage: rm [-d] <file>...\n");
        return 1;
    }
    int failed = 0;
    for (int i = 1 + dirs; i < argc; i++) {
        long r = unlink(argv[i]);
        if (r == -EISDIR && dirs)
            r = rmdir(argv[i]);
        if (r < 0) {
            print("rm: ");
            print(argv[i]);
            print(r == -EISDIR      ? ": is a directory (rm -d removes empty ones)\n"
                  : r == -ENOTEMPTY ? ": directory not empty\n"
                  : r == -EROFS     ? ": read-only (only /tmp is writable)\n"
                                    : ": no such file\n");
            failed = 1;
        }
    }
    return failed;
}
