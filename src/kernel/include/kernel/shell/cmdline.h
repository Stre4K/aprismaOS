#ifndef __KERNEL_SHELL_CMDLINE_H__
#define __KERNEL_SHELL_CMDLINE_H__

#include <stdbool.h>

// Line editing for console input; Enter passes the line to the console
void cmdline_init(void);
void cmdline_input_char(char c);
void cmdline_cursor_left(void);
void cmdline_cursor_right(void);
void cmdline_redraw(void);
void cmdline_home(void);
void cmdline_end(void);
void cmdline_delete(void);
void cmdline_cancel(void);
bool cmdline_is_empty(void);
void cmdline_screen_cleared(void);

#endif
