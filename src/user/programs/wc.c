// wc: count lines, words and bytes of each file named on the command line,
// or of the input until its end (Ctrl+D on the keyboard)
#include "ulib.h"

static unsigned long total_lines, total_words, total_bytes;

static void print_counts(unsigned long lines, unsigned long words, unsigned long bytes, const char *name)
{
    print_int((long)lines);
    print(" ");
    print_int((long)words);
    print(" ");
    print_int((long)bytes);
    if (name) {
        print(" ");
        print(name);
    }
    print("\n");
}

static int count(long fd, const char *name)
{
    char buf[512];
    unsigned long lines = 0, words = 0, bytes = 0;
    int in_word = 0;
    long n;
    while ((n = read((int)fd, buf, sizeof(buf))) > 0) {
        bytes += (unsigned long)n;
        for (long i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n')
                lines++;
            int space = c == ' ' || c == '\t' || c == '\n';
            if (!space && !in_word)
                words++;
            in_word = !space;
        }
    }
    print_counts(lines, words, bytes, name);
    total_lines += lines;
    total_words += words;
    total_bytes += bytes;
    return n < 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return count(0, 0);

    int failed = 0;
    for (int i = 1; i < argc; i++) {
        long fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            eprint("wc: ");
            eprint(argv[i]);
            eprint(": no such file\n");
            failed = 1;
            continue;
        }
        failed |= count(fd, argv[i]);
        close((int)fd);
    }
    if (argc > 2)
        print_counts(total_lines, total_words, total_bytes, "total");
    return failed;
}
