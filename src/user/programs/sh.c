// sh: the user-space shell. Reads a line, splits it into words and runs the
// program named by the first word (looked up in /bin) with the words as its
// arguments, then waits for it. Words are separated by spaces; quotes
// ("..." or '...') keep spaces and the characters | < > inside a word.
//
//   a | b | c      a pipeline: each program's output is the next one's input
//   a < file       the first program reads the file instead of the keyboard
//
// All programs of a pipeline form one process group, so Ctrl+C ends all of
// them. Built in: exit [code], help. Ctrl+C at the prompt starts a new
// line, Ctrl+D at the prompt exits the shell.
#include "ulib.h"

#define LINE_MAX   256
#define WORDS_MAX  32  // the kernel's limit on arguments
#define STAGES_MAX 8   // programs in one pipeline

typedef struct {
    char *argv[WORDS_MAX + 1];
    int argc;
} command_t;

static void error(const char *a, const char *b)
{
    print("sh: ");
    print(a);
    print(b);
    print("\n");
}

/* ---- reading lines (from the keyboard, or from a pipe or file) ---- */

static char input[LINE_MAX];
static long input_length;
static int input_ended;

// The next line without its '\n': 1, 0 at the end of input, -1 when
// Ctrl+C interrupted the wait. Longer lines are cut at LINE_MAX - 1.
static int read_line(char *line)
{
    for (;;) {
        for (long i = 0; i < input_length; i++) {
            if (input[i] == '\n' || i == LINE_MAX - 2) {
                long keep = input[i] == '\n' ? i : i + 1;  // the cut-off case keeps input[i]
                memcpy(line, input, (size_t)keep);
                line[keep] = '\0';
                input_length -= i + 1;
                memcpy(input, input + i + 1, (size_t)input_length);
                return 1;
            }
        }
        if (input_ended) {
            if (input_length == 0)
                return 0;
            memcpy(line, input, (size_t)input_length);  // a last line without '\n'
            line[input_length] = '\0';
            input_length = 0;
            return 1;
        }

        long n = read(0, input + input_length, sizeof(input) - 1 - (size_t)input_length);
        if (n == -EINTR)
            return -1;
        if (n <= 0)
            input_ended = 1;
        else
            input_length += n;
    }
}

/* ---- parsing ---- */

static char words[2 * LINE_MAX];  // the words, each NUL-terminated

// Split a line into commands; returns how many (0 for an empty line, -1
// after printing an error). *input_file is set for "< file".
static int parse(const char *line, command_t cmds[STAGES_MAX], char **input_file)
{
    int count = 1;
    command_t *cmd = &cmds[0];
    char *out = words;
    int expect_file = 0;  // the next word follows '<'
    cmd->argc = 0;
    *input_file = 0;

    for (const char *in = line;;) {
        while (*in == ' ' || *in == '\t')
            in++;
        char c = *in;

        if (c == '\0' || c == '|') {
            if (expect_file) {
                error("'<' needs a file name", "");
                return -1;
            }
            if (cmd->argc == 0) {
                if (c == '\0' && count == 1 && !*input_file)
                    return 0;  // empty line
                error(c == '|' || count > 1 ? "syntax error near '|'" : "a program name is missing", "");
                return -1;
            }
            cmd->argv[cmd->argc] = 0;
            if (c == '\0')
                return count;
            if (count == STAGES_MAX) {
                error("too many programs in one pipeline", "");
                return -1;
            }
            cmd = &cmds[count++];
            cmd->argc = 0;
            in++;
            continue;
        }
        if (c == '>') {
            error("writing to files is not supported yet (the filesystem is read-only)", "");
            return -1;
        }
        if (c == '<') {
            if (count > 1 || *input_file || expect_file) {
                error("only the first program of a pipeline can read from a file", "");
                return -1;
            }
            expect_file = 1;
            in++;
            continue;
        }

        // A word, up to an unquoted space or operator
        char *word = out;
        while (*in && *in != ' ' && *in != '\t' && *in != '|' && *in != '<' && *in != '>') {
            if (*in == '"' || *in == '\'') {
                char quote = *in++;
                while (*in && *in != quote)
                    *out++ = *in++;
                if (*in != quote) {
                    error("missing closing quote", "");
                    return -1;
                }
                in++;
            } else {
                *out++ = *in++;
            }
        }
        *out++ = '\0';

        if (expect_file) {
            *input_file = word;
            expect_file = 0;
        } else if (cmd->argc == WORDS_MAX) {
            error("too many words", "");
            return -1;
        } else {
            cmd->argv[cmd->argc++] = word;
        }
    }
}

/* ---- running ---- */

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
          "Programs live in /bin (see 'ls /bin'). Also:\n"
          "  a | b        a's output is b's input (e.g. 'ls /bin | sort')\n"
          "  a < file     a reads the file (e.g. 'sort < /etc/motd')\n"
          "Built in:\n"
          "  exit [code]  leave the shell\n"
          "  help         show this text\n");
}

static void report_spawn_error(const char *name, long error_code)
{
    if (error_code == -ENOENT)
        error(name, ": command not found");
    else if (error_code == -E2BIG)
        error(name, ": argument list too long");
    else
        error(name, ": cannot run this program");
}

static void run(command_t cmds[], int count, const char *input_file)
{
    long pids[STAGES_MAX];
    const char *names[STAGES_MAX];
    int started = 0;
    int pgid = 0;  // the first program started leads the group

    int in = 0;  // stdin of the next program
    if (input_file) {
        long fd = open(input_file);
        if (fd < 0) {
            error(input_file, fd == -EISDIR ? ": is a directory" : ": no such file");
            return;
        }
        in = (int)fd;
    }

    for (int i = 0; i < count; i++) {
        int out = 1;
        int ends[2];
        if (i < count - 1) {
            if (pipe(ends) < 0) {
                error("cannot create a pipe", "");
                break;
            }
            out = ends[1];
        }

        spawn_options_t options = { .fd = { in, out, 2 }, .pgid = pgid };
        long pid = spawn(cmds[i].argv[0], cmds[i].argv, &options);

        // The children have their own copies now. Closing ours means a
        // reader sees the end once its writer exits, and a writer gets an
        // error once its reader is gone.
        if (in != 0)
            close(in);
        if (out != 1)
            close(out);
        in = i < count - 1 ? ends[0] : 0;

        if (pid < 0) {
            report_spawn_error(cmds[i].argv[0], pid);
            continue;
        }
        pids[started] = pid;
        names[started++] = cmds[i].argv[0];
        if (pgid == 0)
            pgid = (int)pid;
    }
    if (in != 0)
        close(in);  // a pipe left over after an error

    for (int i = 0; i < started; i++) {
        int code;
        long how = wait((int)pids[i], &code);
        // A killed program was already reported by the kernel; stay quiet
        // after a normal exit with code 0
        if (how == 0 && code != 0) {
            print("sh: ");
            print(names[i]);
            print(" exited with code ");
            print_int(code);
            print("\n");
        }
    }
}

int main(void)
{
    char line[LINE_MAX];
    command_t cmds[STAGES_MAX];
    char *input_file;

    for (;;) {
        print("$ ");
        int got = read_line(line);
        if (got < 0)
            continue;  // Ctrl+C at the prompt
        if (got == 0) {
            print("exit\n");  // Ctrl+D (end of input)
            return 0;
        }

        int count = parse(line, cmds, &input_file);
        if (count <= 0)
            continue;

        char *name = cmds[0].argv[0];
        int builtin = strcmp(name, "exit") == 0 || strcmp(name, "help") == 0;
        if (builtin && (count > 1 || input_file)) {
            error(name, ": built-in commands cannot be part of a pipeline");
            continue;
        }
        if (strcmp(name, "exit") == 0)
            return cmds[0].argc > 1 ? to_int(cmds[0].argv[1]) : 0;
        if (strcmp(name, "help") == 0)
            help();
        else
            run(cmds, count, input_file);
    }
}
