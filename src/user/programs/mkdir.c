// mkdir: make directories (only in writable places, i.e. /tmp)
#include "ulib.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        eprint("usage: mkdir <directory>...\n");
        return 1;
    }
    int failed = 0;
    for (int i = 1; i < argc; i++) {
        long r = mkdir(argv[i]);
        if (r < 0) {
            eprint("mkdir: ");
            eprint(argv[i]);
            eprint(r == -EEXIST ? ": already exists\n" : r == -EROFS ? ": read-only (only /tmp is writable)\n"
                                                                    : ": no such directory\n");
            failed = 1;
        }
    }
    return failed;
}
