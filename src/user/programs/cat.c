// cat: print each file named on the command line, or copy the keyboard
// input to the screen until Ctrl+D when there are none
#include "ulib.h"

static int copy(long fd)
{
    char buf[256];
    long n;
    while ((n = read((int)fd, buf, sizeof(buf))) > 0)
        write(1, buf, (size_t)n);
    return n < 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return copy(0);

    int failed = 0;
    for (int i = 1; i < argc; i++) {
        long fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            print("cat: ");
            print(argv[i]);
            print(fd == -EISDIR ? ": is a directory\n" : ": no such file\n");
            failed = 1;
            continue;
        }
        failed |= copy(fd);
        close((int)fd);
    }
    return failed;
}
