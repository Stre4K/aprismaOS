// sh: the user-space shell. Reads a line, splits it into words and runs the
// program named by the first word (looked up in /bin) with the words as its
// arguments, then waits for it. Words are separated by spaces; quotes
// ("..." or '...') keep spaces inside a word.
//
// Built in: exit [code], help. Ctrl+C at the prompt starts a new line,
// Ctrl+C while a program runs ends that program, Ctrl+D at the prompt
// exits the shell.
#include "ulib.h"

#define LINE_MAX  256
#define WORDS_MAX 32  // the kernel's limit on arguments

// Split `line` into words in place; returns the number of words, -1 for an
// unterminated quote or -2 for too many words
static int split(char *line, char *words[WORDS_MAX + 1])
{
    int count = 0;
    char *in = line;

    for (;;) {
        while (*in == ' ' || *in == '\t')
            in++;
        if (*in == '\0')
            break;
        if (count == WORDS_MAX)
            return -2;

        // Copy the word over itself, dropping the quote characters
        char *out = in;
        words[count++] = out;
        while (*in && *in != ' ' && *in != '\t') {
            if (*in == '"' || *in == '\'') {
                char quote = *in++;
                while (*in && *in != quote)
                    *out++ = *in++;
                if (*in != quote)
                    return -1;
                in++;
            } else {
                *out++ = *in++;
            }
        }
        if (*in)
            in++;  // the separator; safe to overwrite now
        *out = '\0';
    }
    words[count] = 0;
    return count;
}

static int to_int(const char *s)
{
    int sign = 1, value = 0;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    while (*s >= '0' && *s <= '9')
        value = value * 10 + (*s++ - '0');
    return sign * value;
}

static void help(void)
{
    print("Type a program name and its arguments, e.g. 'cat /etc/motd'.\n"
          "Programs live in /bin (see 'ls /bin'). Built in:\n"
          "  exit [code]  leave the shell\n"
          "  help         show this text\n");
}

static void run(char *words[])
{
    long pid = spawn(words[0], words);
    if (pid < 0) {
        print("sh: ");
        print(words[0]);
        if (pid == -ENOENT)
            print(": command not found\n");
        else if (pid == -E2BIG)
            print(": argument list too long\n");
        else
            print(": cannot run this program\n");
        return;
    }

    int code;
    long how = wait((int)pid, &code);
    // A killed program was already reported by the kernel; stay quiet
    // after a normal exit with code 0
    if (how == 0 && code != 0) {
        print("sh: ");
        print(words[0]);
        print(" exited with code ");
        print_int(code);
        print("\n");
    }
}

int main(void)
{
    char line[LINE_MAX];
    char *words[WORDS_MAX + 1];

    for (;;) {
        print("$ ");
        long n = read(0, line, sizeof(line) - 1);
        if (n == -EINTR)
            continue;  // Ctrl+C at the prompt
        if (n <= 0) {
            print("exit\n");  // Ctrl+D (end of input)
            return 0;
        }
        if (line[n - 1] == '\n')
            n--;
        line[n] = '\0';

        int count = split(line, words);
        if (count == -1) {
            print("sh: missing closing quote\n");
            continue;
        }
        if (count == -2) {
            print("sh: too many words\n");
            continue;
        }
        if (count == 0)
            continue;

        if (strcmp(words[0], "exit") == 0)
            return count > 1 ? to_int(words[1]) : 0;
        if (strcmp(words[0], "help") == 0)
            help();
        else
            run(words);
    }
}
