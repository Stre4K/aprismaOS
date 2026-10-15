// runner.c
// Runs every registered test. Each test gets a time limit and is protected
// against crashes, so a hang or segfault in the code under test is reported
// as a failure instead of stopping the run.
//
// Usage: unit-tests [filter]   (runs only tests whose name contains filter)

#include "test.h"

#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_TESTS 512
#define TEST_TIMEOUT_SECONDS 2

typedef struct {
    const char *file;
    const char *name;
    test_fn_t fn;
    const char *known_bug;
} test_case_t;

static test_case_t tests[MAX_TESTS];
static int test_count;

static int current_failed;
static char failure_message[512];

static sigjmp_buf test_escape;
static volatile sig_atomic_t escape_signal;

void test_register(const char *file, const char *name, test_fn_t fn, const char *known_bug)
{
    if (test_count == MAX_TESTS) {
        fprintf(stderr, "too many tests, raise MAX_TESTS\n");
        exit(2);
    }
    const char *base = strrchr(file, '/');
    tests[test_count++] = (test_case_t){ base ? base + 1 : file, name, fn, known_bug };
}

void test_fail(const char *file, int line, const char *fmt, ...)
{
    if (current_failed)
        return;
    current_failed = 1;

    const char *base = strrchr(file, '/');
    int n = snprintf(failure_message, sizeof(failure_message), "%s:%d: ", base ? base + 1 : file, line);

    va_list args;
    va_start(args, fmt);
    vsnprintf(failure_message + n, sizeof(failure_message) - n, fmt, args);
    va_end(args);
}

static void on_signal(int sig)
{
    escape_signal = sig;
    siglongjmp(test_escape, 1);
}

// Run one test; returns 1 if it passed
static int run_test(const test_case_t *t)
{
    current_failed = 0;
    failure_message[0] = '\0';
    escape_signal = 0;

    if (sigsetjmp(test_escape, 1) == 0) {
        alarm(TEST_TIMEOUT_SECONDS);
        t->fn();
        alarm(0);
    } else {
        alarm(0);
        current_failed = 1;
        if (escape_signal == SIGALRM)
            snprintf(failure_message, sizeof(failure_message),
                     "timed out after %d s (hang?)", TEST_TIMEOUT_SECONDS);
        else
            snprintf(failure_message, sizeof(failure_message),
                     "crashed with signal %d (%s)", escape_signal, strsignal(escape_signal));
    }

    return !current_failed;
}

int main(int argc, char **argv)
{
    const char *filter = argc > 1 ? argv[1] : NULL;

    // A separate signal stack lets stack overflows be reported too
    static char alt_stack[64 * 1024];
    stack_t ss = { .ss_sp = alt_stack, .ss_size = sizeof(alt_stack) };
    sigaltstack(&ss, NULL);

    struct sigaction sa = { .sa_handler = on_signal, .sa_flags = SA_ONSTACK };
    sigemptyset(&sa.sa_mask);
    int signals[] = { SIGALRM, SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };
    for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++)
        sigaction(signals[i], &sa, NULL);

    int passed = 0, failed = 0, known = 0, fixed = 0;

    for (int i = 0; i < test_count; i++) {
        const test_case_t *t = &tests[i];
        if (filter && !strstr(t->name, filter))
            continue;

        int ok = run_test(t);

        if (t->known_bug == NULL) {
            if (ok) {
                passed++;
                printf("PASS        %s: %s\n", t->file, t->name);
            } else {
                failed++;
                printf("FAIL        %s: %s\n            %s\n", t->file, t->name, failure_message);
            }
        } else {
            if (ok) {
                fixed++;
                printf("FIXED?      %s: %s\n            passes now; remove TEST_KNOWN_BUG (\"%s\")\n",
                       t->file, t->name, t->known_bug);
            } else {
                known++;
                printf("KNOWN BUG   %s: %s\n            %s\n            %s\n",
                       t->file, t->name, t->known_bug, failure_message);
            }
        }
    }

    printf("\n%d passed, %d failed, %d known bugs, %d known bugs now passing\n",
           passed, failed, known, fixed);

    return (failed || fixed) ? 1 : 0;
}
