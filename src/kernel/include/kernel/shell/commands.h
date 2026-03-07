// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-06

#ifndef __KERNEL_SHELL_COMMANDS_H__
#define __KERNEL_SHELL_COMMANDS_H__


typedef struct {
    const char *name;
    const char *description;
    void (*func)(int argc, char **argv);
} shell_command_t;

void cmd_version(int argc, char **argv);

void cmd_clear(int argc, char **argv);
void cmd_echo(int argc, char **argv);
void cmd_help(int argc, char **argv);

void cmd_reboot(int argc, char **argv);
void cmd_uptime(int argc, char **argv);
void cmd_cpuid(int argc, char **argv);
void cmd_panic(int argc, char **argv);
void cmd_mem(int argc, char **argv);
void cmd_stack(int argc, char **argv);

void cmd_test(int argc, char **argv);

void init_shell_commands();

#endif // KERNEL_SHELL_COMMANDS_H
