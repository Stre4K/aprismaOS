// cpuid.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-14

#include <kernel/config.h>

#ifdef CONFIG_CMD_CPU

#include <kernel/shell/commands.h>

#include <kernel/arch/cpuid.h>

void cmd_cpuid(int argc, char **argv){
    (void)argc;
    (void)argv;
    cpuid_detect_vendor();
    cpuid_detect_features_print();
}


SHELL_CMD("cpu", "cpuid", "Display cpuid information", cmd_cpuid);

#endif // CONFIG_CMD_CPU
