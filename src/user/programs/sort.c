// sort: print lines in sorted order. Reads the files named on the command
// line, or the keyboard until Ctrl+D. Every line is kept on the heap, so
// there is no limit on how many there are.
#include "ulib.h"

static char **lines;
static size_t count, capacity;

static int add_line(const char *text, size_t len)
{
    if (count == capacity) {
        size_t bigger = capacity ? capacity * 2 : 16;
        char **grown = realloc(lines, bigger * sizeof(*lines));
        if (grown == 0)
            return 0;
        lines = grown;
        capacity = bigger;
    }
    char *copy = malloc(len + 1);
    if (copy == 0)
        return 0;
    memcpy(copy, text, len);
    copy[len] = '\0';
    lines[count++] = copy;
    return 1;
}

// Read everything from fd, splitting it into lines; 0 if memory ran out
static int read_lines(long fd)
{
    char buf[256];
    char *partial = 0;  // a line split across reads
    size_t partial_len = 0;
    long n;

    while ((n = read((int)fd, buf, sizeof(buf))) > 0) {
        char *grown = realloc(partial, partial_len + (size_t)n + 1);
        if (grown == 0)
            return 0;
        partial = grown;
        memcpy(partial + partial_len, buf, (size_t)n);
        partial_len += (size_t)n;

        size_t start = 0;
        for (size_t i = 0; i < partial_len; i++) {
            if (partial[i] == '\n') {
                if (!add_line(partial + start, i - start))
                    return 0;
                start = i + 1;
            }
        }
        partial_len -= start;
        for (size_t i = 0; i < partial_len; i++)
            partial[i] = partial[start + i];
    }
    int ok = partial_len == 0 || add_line(partial, partial_len);  // last line without '\n'
    free(partial);
    return ok;
}

// Merge sort through a scratch array (stable, n log n)
static void sort_lines(char **a, char **scratch, size_t n)
{
    if (n < 2)
        return;
    size_t half = n / 2;
    sort_lines(a, scratch, half);
    sort_lines(a + half, scratch, n - half);
    size_t i = 0, j = half, k = 0;
    while (i < half && j < n)
        scratch[k++] = strcmp(a[j], a[i]) < 0 ? a[j++] : a[i++];
    while (i < half)
        scratch[k++] = a[i++];
    while (j < n)
        scratch[k++] = a[j++];
    memcpy(a, scratch, n * sizeof(*a));
}

int main(int argc, char **argv)
{
    int ok = 1;
    if (argc < 2) {
        ok = read_lines(0);
    } else {
        for (int i = 1; i < argc && ok; i++) {
            long fd = open(argv[i]);
            if (fd < 0) {
                print("sort: ");
                print(argv[i]);
                print(": cannot open\n");
                return 1;
            }
            ok = read_lines(fd);
            close((int)fd);
        }
    }

    char **scratch = malloc((count ? count : 1) * sizeof(*scratch));
    if (!ok || scratch == 0) {
        print("sort: out of memory\n");
        return 2;
    }
    sort_lines(lines, scratch, count);
    for (size_t i = 0; i < count; i++) {
        print(lines[i]);
        print("\n");
        free(lines[i]);
    }
    free(scratch);
    free(lines);
    return 0;
}
