// console.h
// Console input: a thread owns the keyboard and does the line editing;
// finished lines wait in a queue until the shell or a program reads them.

#ifndef __KERNEL_CONSOLE_H__
#define __KERNEL_CONSOLE_H__

#include <stddef.h>

#define CONSOLE_LINE_MAX 128

// Start the console thread, which handles the keyboard from then on
void console_start(void);

// Read input, a line at a time: blocks until a whole line was entered,
// then copies up to `len` bytes of it including the '\n' (the rest of a
// longer line stays for the next read). Returns the number of bytes, 0 at
// end of input (Ctrl+D), or -EINTR if the calling process is being killed
// or Ctrl+C was pressed while it waited.
long console_read(char *buf, size_t len);

// From the line editor
void console_line_entered(const char *line, size_t len);
void console_eof(void);        // Ctrl+D on an empty line
void console_interrupt(void);  // Ctrl+C

#ifdef CONFIG_USERMODE
// The process group Ctrl+C ends (0 for none)
void console_set_foreground(int pgid);
int console_get_foreground(void);
#endif

#endif // __KERNEL_CONSOLE_H__
