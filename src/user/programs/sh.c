// sh: the user-space shell. Reads a line, splits it into words and runs the
// program named by the first word (looked up in /bin) with the words as its
// arguments, then waits for it. Words are separated by spaces; quotes
// ("..." or '...') keep spaces and the characters | < > inside a word.
//
//   a | b | c      a pipeline: each program's output is the next one's input
//   a < file       the first program reads the file instead of the keyboard
//   a > file       the last program writes the file instead of the screen
//   a >> file      ... adding to the end of the file
//   a 2> file      a's error messages go to the file (2>> adds, 2>&1 sends
//                  them where its output goes)
//   a &            run in the background: the prompt comes back at once
//
// Background jobs get no keyboard input (an empty input unless "<" gives
// one), are not ended by Ctrl+C, and are reported when they end, before
// the next prompt. jobs lists them, fg waits for one, kill ends one.
//
// Only /tmp is writable. Prompts are only shown when the input is the
// keyboard, so "sh < script" or "... | sh" runs commands quietly.
//
// All programs of a pipeline form one process group, so Ctrl+C ends all of
// them. Relative paths start at the current directory. Built in:
// cd [dir], pwd, jobs, fg [%job], kill %job|pid..., exit [code], help. Ctrl+C at the prompt starts a new
// line, Ctrl+D at the prompt exits the shell.
#include "ulib.h"

#define LINE_MAX   256
#define WORDS_MAX  32  // the kernel's limit on arguments
#define STAGES_MAX 8   // programs in one pipeline
#define JOBS_MAX   8   // background jobs at once

typedef struct {
    char *argv[WORDS_MAX + 1];
    int argc;
    char *err_file;    // "2> file", or NULL
    int err_append;    // "2>>"
    int err_to_out;    // "2>&1"
} command_t;

static void error(const char *a, const char *b)
{
    eprint("sh: ");
    eprint(a);
    eprint(b);
    eprint("\n");
}

static void new_command(command_t *cmd)
{
    cmd->argc = 0;
    cmd->err_file = 0;
    cmd->err_append = cmd->err_to_out = 0;
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

typedef struct {
    char *input;       // "< file", or NULL
    char *output;      // "> file" or ">> file", or NULL
    int append;        // ">>"
    int background;    // the line ends in '&'
} redirect_t;

// Split a line into commands; returns how many (0 for an empty line, -1
// after printing an error), with the redirections in *redir
static int parse(const char *line, command_t cmds[STAGES_MAX], redirect_t *redir)
{
    int count = 1;
    command_t *cmd = &cmds[0];
    char *out = words;
    char **expect_file = 0;  // where the next word goes after '<' or '>'
    int output_stage = -1;   // the program "> file" belongs to
    new_command(cmd);
    redir->input = redir->output = 0;
    redir->append = redir->background = 0;

    for (const char *in = line;;) {
        while (*in == ' ' || *in == '\t')
            in++;
        char c = *in;

        if (c == '\0' || c == '|') {
            if (expect_file) {
                error("a file name is missing", "");
                return -1;
            }
            if (cmd->argc == 0) {
                if (c == '\0' && count == 1 && !redir->input && !redir->output)
                    return 0;  // empty line
                error(c == '|' || count > 1 ? "syntax error near '|'" : "a program name is missing", "");
                return -1;
            }
            cmd->argv[cmd->argc] = 0;
            if (c == '\0') {
                if (output_stage >= 0 && output_stage != count - 1) {
                    error("only the last program of a pipeline can write to a file", "");
                    return -1;
                }
                return count;
            }
            if (count == STAGES_MAX) {
                error("too many programs in one pipeline", "");
                return -1;
            }
            cmd = &cmds[count++];
            new_command(cmd);
            in++;
            continue;
        }
        // '&' at the end: in the background
        if (c == '&') {
            const char *rest = in + 1;
            while (*rest == ' ' || *rest == '\t')
                rest++;
            if (*rest || redir->background) {
                error("'&' can only end a line", "");
                return -1;
            }
            redir->background = 1;
            in = rest;
            continue;
        }

        // "2>" at the start of a word: where this program's errors go
        if (c == '2' && in[1] == '>') {
            if (expect_file) {
                error("a file name is missing", "");
                return -1;
            }
            if (cmd->err_file || cmd->err_to_out) {
                error("only one '2>' per program", "");
                return -1;
            }
            in += 2;
            if (in[0] == '&' && in[1] == '1') {
                cmd->err_to_out = 1;
                in += 2;
                continue;
            }
            cmd->err_append = *in == '>';
            in += cmd->err_append;
            expect_file = &cmd->err_file;
            continue;
        }
        if (c == '>' || c == '<') {
            if (expect_file) {
                error("a file name is missing", "");
                return -1;
            }
            if (c == '<' && (count > 1 || redir->input)) {
                error("only the first program of a pipeline can read from a file", "");
                return -1;
            }
            if (c == '>' && redir->output) {
                error("only one '>' per line", "");
                return -1;
            }
            if (c == '>') {
                redir->append = in[1] == '>';
                in += redir->append;
                output_stage = count - 1;
            }
            expect_file = c == '<' ? &redir->input : &redir->output;
            in++;
            continue;
        }

        // A word, up to an unquoted space or operator
        char *word = out;
        while (*in && *in != ' ' && *in != '\t' && *in != '|' && *in != '<' && *in != '>' && *in != '&') {
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
            *expect_file = word;
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
          "  a > file     a writes the file, only in /tmp (e.g. 'ls /bin > /tmp/list')\n"
          "  a >> file    a adds to the end of the file\n"
          "  a 2> file    a's error messages go to the file (2>> adds, 2>&1 joins the output)\n"
          "  a &          run a in the background (no keyboard input, Ctrl+C leaves it alone)\n"
          "Paths not starting with / are relative to the current directory.\n"
          "Built in:\n"
          "  cd [dir]     change the current directory (to / without dir)\n"
          "  pwd          show the current directory\n"
          "  jobs         list the background jobs\n"
          "  fg [%n]      wait for a background job (the newest without %n)\n"
          "  kill %n|pid  end a job or a process\n"
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

static void file_error(const char *path, long code)
{
    if (code == -EISDIR)
        error(path, ": is a directory");
    else if (code == -EROFS)
        error(path, ": read-only (only /tmp is writable)");
    else if (code == -ENOTDIR)
        error(path, ": not a directory");
    else if (code == -ENOSPC)
        error(path, ": no space left");
    else
        error(path, ": no such file or directory");
}

/* ---- jobs ---- */

typedef struct {
    int id;                       // [1], [2], ...; 0 for a free slot
    int count;                    // programs started
    int pgid;
    long pids[STAGES_MAX];
    char names[STAGES_MAX][32];   // copies: the words are reused by the next line
    int done[STAGES_MAX];
    long hows[STAGES_MAX];        // wait()'s result for each
    int codes[STAGES_MAX];
    char text[LINE_MAX];          // the command, for jobs and reports
} job_t;

static job_t jobs[JOBS_MAX];

// Collect the programs of a job that have ended (waiting for all of them
// if `block`); returns 1 once every one has
static int collect(job_t *job, int block)
{
    int all = 1;
    for (int i = 0; i < job->count; i++) {
        if (job->done[i])
            continue;
        long how = block ? wait((int)job->pids[i], &job->codes[i])
                         : waitpid((int)job->pids[i], &job->codes[i], WAIT_NOHANG);
        if (how == -EAGAIN) {
            all = 0;
            continue;
        }
        job->done[i] = 1;
        job->hows[i] = how == -ECHILD ? 0 : how;
    }
    return all;
}

// After a foreground job: the programs that failed. A killed program was
// already reported by the kernel; stay quiet after exit code 0.
static void report_codes(const job_t *job)
{
    for (int i = 0; i < job->count; i++) {
        if (job->hows[i] == 0 && job->codes[i] != 0) {
            eprint("sh: ");
            eprint(job->names[i]);
            eprint(" exited with code ");
            eprint_int(job->codes[i]);
            eprint("\n");
        }
    }
}

// "[1] done  sleep 1", "[1] exit 2  cat nope" or "[1] killed  ..."
static void report_job(const job_t *job)
{
    int killed = 0, code = 0;
    for (int i = 0; i < job->count; i++) {
        killed |= job->hows[i] == WAIT_KILLED;
        if (job->hows[i] == 0 && job->codes[i] != 0)
            code = job->codes[i];
    }
    print("[");
    print_int(job->id);
    if (killed) {
        print("] killed  ");
    } else if (code) {
        print("] exit ");
        print_int(code);
        print("  ");
    } else {
        print("] done  ");
    }
    print(job->text);
    print("\n");
}

// Report and forget the background jobs that have ended
static void reap_jobs(void)
{
    for (int i = 0; i < JOBS_MAX; i++) {
        if (jobs[i].id && collect(&jobs[i], 0)) {
            report_job(&jobs[i]);
            jobs[i].id = 0;
        }
    }
}

// The job for "%n" (or the newest for NULL), or NULL
static job_t *find_job(const char *ref)
{
    job_t *newest = 0;
    for (int i = 0; i < JOBS_MAX; i++) {
        if (!jobs[i].id)
            continue;
        if (ref == 0) {
            if (!newest || jobs[i].id > newest->id)
                newest = &jobs[i];
        } else if (ref[0] == '%' && strcmp(ref + 1, "") != 0) {
            int id = 0;
            for (const char *p = ref + 1; *p >= '0' && *p <= '9'; p++)
                id = id * 10 + (*p - '0');
            if (jobs[i].id == id)
                return &jobs[i];
        }
    }
    return newest;
}

static void builtin_jobs(void)
{
    reap_jobs();
    for (int i = 0; i < JOBS_MAX; i++) {
        if (jobs[i].id) {
            print("[");
            print_int(jobs[i].id);
            print("] running  ");
            print(jobs[i].text);
            print("\n");
        }
    }
}

static void builtin_fg(const char *ref)
{
    job_t *job = find_job(ref);
    if (job == 0) {
        error(ref ? ref : "fg", ": no such job");
        return;
    }
    print(job->text);
    print("\n");
    collect(job, 1);  // wait() also hands it the keyboard and Ctrl+C
    report_codes(job);
    job->id = 0;
}

static void builtin_kill(char **args, int argc)
{
    if (argc < 2) {
        error("usage: kill %job|pid...", "");
        return;
    }
    for (int i = 1; i < argc; i++) {
        long r;
        if (args[i][0] == '%') {
            job_t *job = find_job(args[i]);
            r = job ? kill(-job->pgid) : -ESRCH;
        } else {
            int pid = 0;
            for (const char *p = args[i]; *p >= '0' && *p <= '9'; p++)
                pid = pid * 10 + (*p - '0');
            r = pid > 0 ? kill(pid) : -ESRCH;
        }
        if (r < 0)
            error(args[i], args[i][0] == '%' ? ": no such job" : ": no such process");
    }
}

/* ---- running ---- */

static void run(command_t cmds[], int count, const redirect_t *redir, const char *line)
{
    static job_t job;
    memset(&job, 0, sizeof(job));
    long *pids = job.pids;
    int started = 0;
    int pgid = 0;  // the first program started leads the group

    int in = 0;  // stdin of the next program
    if (redir->background && !redir->input) {
        // An empty input, like /dev/null elsewhere: a pipe nobody writes
        int ends[2];
        if (pipe(ends) == 0) {
            close(ends[1]);
            in = ends[0];
        } else {
            in = -1;  // no input at all
        }
    }
    if (redir->input) {
        long fd = open(redir->input, O_RDONLY);
        if (fd < 0) {
            file_error(redir->input, fd);
            return;
        }
        in = (int)fd;
    }
    int last_out = 1;  // stdout of the last program
    if (redir->output) {
        long fd = open(redir->output, O_WRONLY | O_CREAT | (redir->append ? O_APPEND : O_TRUNC));
        if (fd < 0) {
            file_error(redir->output, fd);
            if (in > 0)
                close(in);
            return;
        }
        last_out = (int)fd;
    }

    for (int i = 0; i < count; i++) {
        int out = last_out;
        int ends[2];
        if (i < count - 1) {
            if (pipe(ends) < 0) {
                error("cannot create a pipe", "");
                break;
            }
            out = ends[1];
        }

        // Its error messages: stderr, its output, or a file
        int err = cmds[i].err_to_out ? out : 2;
        long pid = 0;
        if (cmds[i].err_file) {
            long fd = open(cmds[i].err_file,
                           O_WRONLY | O_CREAT | (cmds[i].err_append ? O_APPEND : O_TRUNC));
            if (fd < 0)
                file_error(cmds[i].err_file, fd);
            pid = fd < 0 ? -1 : 0;
            err = fd < 0 ? 2 : (int)fd;
        }

        spawn_options_t options = { .fd = { in, out, err }, .pgid = pgid };
        if (pid == 0)
            pid = spawn(cmds[i].argv[0], cmds[i].argv, &options);
        else
            pid = 0;  // reported above; this program does not run
        if (err != 2 && err != out)
            close(err);

        // The children have their own copies now. Closing ours means a
        // reader sees the end once its writer exits, and a writer gets an
        // error once its reader is gone.
        if (in > 0)
            close(in);
        if (out != 1)
            close(out);
        if (out == last_out)
            last_out = 1;  // closed with it
        in = i < count - 1 ? ends[0] : 0;

        if (pid <= 0) {
            if (pid < 0)
                report_spawn_error(cmds[i].argv[0], pid);
            continue;
        }
        pids[started] = pid;
        size_t len = strlen(cmds[i].argv[0]);
        if (len > sizeof(job.names[0]) - 1)
            len = sizeof(job.names[0]) - 1;
        memcpy(job.names[started], cmds[i].argv[0], len);
        job.names[started++][len] = '\0';
        if (pgid == 0)
            pgid = (int)pid;
    }
    if (in > 0)
        close(in);  // a pipe left over after an error
    if (last_out != 1)
        close(last_out);  // the pipeline stopped before its last program
    if (started == 0)
        return;
    job.count = started;
    job.pgid = pgid;

    if (redir->background) {
        for (int i = 0; i < JOBS_MAX; i++) {
            if (jobs[i].id)
                continue;
            // The lowest free number, like other shells
            int id = 1;
            for (int taken = 1; taken;) {
                taken = 0;
                for (int k = 0; k < JOBS_MAX; k++)
                    taken |= jobs[k].id == id;
                id += taken;
            }
            jobs[i] = job;
            jobs[i].id = id;
            size_t len = strlen(line);
            while (len && (line[len - 1] == ' ' || line[len - 1] == '&'))
                len--;  // "sleep 5 &" is shown as "sleep 5"
            memcpy(jobs[i].text, line, len);
            jobs[i].text[len] = '\0';
            print("[");
            print_int(id);
            print("] ");
            print_int(pgid);
            print("\n");
            return;
        }
        error("too many background jobs; waiting for this one", "");
    }

    // Wait for all of them before reporting, so a report cannot land in
    // the middle of another program's output
    collect(&job, 1);
    report_codes(&job);
}

int main(void)
{
    char line[LINE_MAX];
    command_t cmds[STAGES_MAX];
    redirect_t redir;
    int interactive = isatty(0);

    for (;;) {
        reap_jobs();
        if (interactive)
            print("$ ");
        int got = read_line(line);
        if (got < 0)
            continue;  // Ctrl+C at the prompt
        if (got == 0) {
            if (interactive)
                print("exit\n");  // Ctrl+D
            return 0;
        }

        int count = parse(line, cmds, &redir);
        if (count <= 0)
            continue;

        char *name = cmds[0].argv[0];
        int builtin = strcmp(name, "exit") == 0 || strcmp(name, "help") == 0 ||
                      strcmp(name, "cd") == 0 || strcmp(name, "pwd") == 0 ||
                      strcmp(name, "jobs") == 0 || strcmp(name, "fg") == 0 || strcmp(name, "kill") == 0;
        if (builtin && (count > 1 || redir.input || redir.output || redir.background ||
                        cmds[0].err_file || cmds[0].err_to_out)) {
            error(name, ": built-in commands cannot be part of a pipeline, redirected or run with &");
            continue;
        }
        if (strcmp(name, "exit") == 0)
            return cmds[0].argc > 1 ? to_int(cmds[0].argv[1]) : 0;
        if (strcmp(name, "help") == 0) {
            help();
        } else if (strcmp(name, "cd") == 0) {
            const char *dir = cmds[0].argc > 1 ? cmds[0].argv[1] : "/";
            long r = chdir(dir);
            if (r < 0)
                error(dir, r == -ENOTDIR ? ": not a directory" : ": no such directory");
        } else if (strcmp(name, "jobs") == 0) {
            builtin_jobs();
        } else if (strcmp(name, "fg") == 0) {
            builtin_fg(cmds[0].argc > 1 ? cmds[0].argv[1] : 0);
        } else if (strcmp(name, "kill") == 0) {
            builtin_kill(cmds[0].argv, cmds[0].argc);
        } else if (strcmp(name, "pwd") == 0) {
            char cwd[256];
            if (getcwd(cwd, sizeof(cwd)) >= 0) {
                print(cwd);
                print("\n");
            }
        } else {
            run(cmds, count, &redir, line);
        }
    }
}
