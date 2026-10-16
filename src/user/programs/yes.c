// yes: print a line ("y", or the arguments) over and over, until writing
// fails, e.g. because the program reading it through a pipe has ended
#include "ulib.h"

int main(int argc, char **argv)
{
    char line[256];
    size_t len = 0;
    if (argc < 2) {
        line[len++] = 'y';
    }
    for (int i = 1; i < argc; i++) {
        for (const char *s = argv[i]; *s && len < sizeof(line) - 2; s++)
            line[len++] = *s;
        if (i < argc - 1 && len < sizeof(line) - 2)
            line[len++] = ' ';
    }
    line[len++] = '\n';

    // Many copies per write
    char buf[4096];
    size_t fill = 0;
    while (fill + len <= sizeof(buf)) {
        memcpy(buf + fill, line, len);
        fill += len;
    }

    for (;;) {
        long n = write(1, buf, fill);
        if (n < 0)
            return n == -EPIPE ? 0 : 1;  // no reader left: done
    }
}
