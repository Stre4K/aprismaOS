// parser.c
// <kernel/shell/parser.h>
// Author: Stre4K
// Date: 2026-03-06


int shell_parse(char *line, char **argv) {
    int argc = 0;

    while (*line) {
        while (*line == ' ') line++;
        if (*line == 0) break;

        argv[argc++] = line;

        while (*line && *line != ' ') line++;
        if (*line) *line++ = 0;
    }

    return argc;
}

