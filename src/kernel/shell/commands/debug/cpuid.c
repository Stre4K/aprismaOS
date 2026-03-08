// cpuid.c
// <kernel/shell/commands.h>
// Author: Stre4K
// Date: 2026-03-07

#include <kernel/arch/cpuid.h>

void cmd_cpuid(int argc, char **argv){
    (void)argc;
    (void)argv;
    cpuid_detect_vendor();
    cpuid_detect_features_print();
}

