// commands.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-06

#include <kernel/shell/commands.h>
#include <kernel/shell/shell.h>

#include <kernel/drivers/tty.h>
#include <kernel/printk.h>

void cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    terminal_clear_screen();
}

void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        printk("%s ", argv[i]);
    }
    printk("\n");
}

void cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;
    const shell_command_t *cmds = shell_get_commands();
    size_t num_commands = shell_get_num_commands();
    printk("Available commands:\n");
    for (size_t i = 0; i < num_commands; i++) {
        printk("%s - %s\n", cmds[i].name, cmds[i].description ? cmds[i].description : "No description");
    }
}

void init_shell_commands(void) {
    shell_register_command("clear", "Clear the screen", cmd_clear);
    shell_register_command("echo", "Print text", cmd_echo);
    shell_register_command("help", "Show available commands", cmd_help);
   }

