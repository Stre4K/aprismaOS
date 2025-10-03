#ifndef __KERNEL_SHELL_CMDLINE_H__
#define __KERNEL_SHELL_CMDLINE_H__

void cmdline_init(void);
void cmdline_input_char(char c);
void cmdline_cursor_left(void);
void cmdline_cursor_right(void);
void cmdline_redraw(void);

#endif
