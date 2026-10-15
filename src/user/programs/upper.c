// upper: echo each line in upper case until end of input (Ctrl+D);
// the exit code is the number of lines
#include "ulib.h"

int main(void)
{
    char line[128];
    int lines = 0;
    long n;

    while ((n = read(0, line, sizeof(line))) > 0) {
        for (long i = 0; i < n; i++) {
            if (line[i] >= 'a' && line[i] <= 'z')
                line[i] = (char)(line[i] - 'a' + 'A');
        }
        write(1, line, (size_t)n);
        if (line[n - 1] == '\n')
            lines++;
    }
    return lines;
}
