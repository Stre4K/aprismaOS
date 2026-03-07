// commands.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/shell/commands.h>
#include <kernel/shell/shell.h>

void init_shell_commands(void) {
    shell_register_command("version", "Display version information", cmd_version);
    shell_register_command("clear", "Clear the screen", cmd_clear);
    shell_register_command("echo", "Print text", cmd_echo);
    shell_register_command("help", "Show available commands", cmd_help);

    shell_register_command("reboot", "Reboot the system", cmd_reboot);
    shell_register_command("uptime", "Show system uptime", cmd_uptime);
    shell_register_command("cpuid", "Display cpuid information", cmd_cpuid);

    shell_register_command("mem", "Display memory information", cmd_mem);
    shell_register_command("stack", "Display stack information", cmd_stack);

    shell_register_command("panic", "Panic the system", cmd_panic);

    shell_register_command("test", "Kernel test command", cmd_test);
}
