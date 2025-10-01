#include <kernel/arch/cpuid.h>
#include <kernel/printk.h>
#include <stdint.h>
#include <string.h>

extern int cpuid_supported_check_asm(void);
struct cpu_features_t cpu_features;

void cpuid_init() {
    if (!cpuid_supported_check()) {
        return;
    }
    cpuid_detect_vendor();
    cpuid_detect_features();
    cpuid_detect_features_print();
}


// Check if CPUID is supported, returns 0 if not supported
uint32_t cpuid_supported_check(void) {
    return cpuid_supported_check_asm();
}

void cpuid_detect_vendor() {
    cpuid_info_t info;
    cpuid_leaf(0, &info);

    for (uint32_t i = 0; i < sizeof(cpu_vendors) / sizeof(cpu_vendors[0]); i++) {
        if (info.ebx == cpu_vendors[i].ebx &&
            info.edx == cpu_vendors[i].edx &&
            info.ecx == cpu_vendors[i].ecx) {
            printk("[CPUID] CPU: %s\n", cpu_vendors[i].name);
            return;
        }
    }
}

void cpuid_detect_features() {
    cpuid_info_t info;

    // ==================================
    // --- Basic information (leaf 0) ---
    // ==================================
    cpuid_leaf(0, &info);

    // Get basic information
    cpu_features.max_basic_leaf = info.eax;

    // Get vendor
    memcpy(cpu_features.vendor, &info.ebx, sizeof(info.ebx));
    memcpy(cpu_features.vendor + 4, &info.edx, sizeof(info.edx));
    memcpy(cpu_features.vendor + 8, &info.ecx, sizeof(info.ecx));
    cpu_features.vendor[12] = '\0';


    // =====================================
    // --- Extended information (leaf 1) ---
    // =====================================
    cpuid_leaf(1, &info);

    // EDX
    cpu_features.apic = (info.edx & bit_APIC);

    // =====================================
    // --- Extended information (leaf 0x80000000) ---
    // =====================================
    cpuid_leaf(0x80000000, &info);
    cpu_features.max_extended_leaf = info.eax;

}


void cpuid_detect_features_print() {
    cpuid_info_t info;


    printk("=== CPU Feature Detection ===\n");

    // --- Basic feature flags (leaf 1) ---
    cpuid_leaf(1, &info);

    printk("[ECX features]:\n");
    if (info.ecx & bit_SSE3)      printk("  - SSE3\n");
    if (info.ecx & bit_PCLMUL)    printk("  - PCLMULQDQ\n");
    if (info.ecx & bit_SSSE3)     printk("  - SSSE3\n");
    if (info.ecx & bit_FMA)       printk("  - FMA\n");
    if (info.ecx & bit_SSE4_1)    printk("  - SSE4.1\n");
    if (info.ecx & bit_SSE4_2)    printk("  - SSE4.2\n");
    if (info.ecx & bit_MOVBE)     printk("  - MOVBE\n");
    if (info.ecx & bit_POPCNT)    printk("  - POPCNT\n");
    if (info.ecx & bit_AES)       printk("  - AES\n");
    if (info.ecx & bit_XSAVE)     printk("  - XSAVE\n");
    if (info.ecx & bit_OSXSAVE)   printk("  - OSXSAVE\n");
    if (info.ecx & bit_AVX)       printk("  - AVX\n");
    if (info.ecx & bit_F16C)      printk("  - F16C\n");
    if (info.ecx & bit_RDRND)     printk("  - RDRAND\n");

    printk("[EDX features]:\n");
    if (info.edx & bit_APIC)      printk("  - APIC\n");
    if (info.edx & bit_CMOV)      printk("  - CMOV\n");
    if (info.edx & bit_MMX)       printk("  - MMX\n");
    if (info.edx & bit_FXSAVE)    printk("  - FXSAVE/FXRSTOR\n");
    if (info.edx & bit_SSE)       printk("  - SSE\n");
    if (info.edx & bit_SSE2)      printk("  - SSE2\n");

    // --- Extended feature flags (leaf 0x80000001) ---
    cpuid_leaf(0x80000001, &info);

    printk("[Extended ECX features]:\n");
    if (info.ecx & bit_ABM)       printk("  - ABM (Advanced Bit Manipulation)\n");
    if (info.ecx & bit_SSE4a)     printk("  - SSE4a\n");
    if (info.ecx & bit_PRFCHW)    printk("  - PREFETCHW\n");
    if (info.ecx & bit_XOP)       printk("  - XOP\n");
    if (info.ecx & bit_FMA4)      printk("  - FMA4\n");
    if (info.ecx & bit_TBM)       printk("  - TBM\n");

    printk("[Extended EDX features]:\n");
    if (info.edx & bit_LM)        printk("  - Long Mode (x86-64)\n");
    if (info.edx & bit_3DNOW)     printk("  - 3DNow!\n");
    if (info.edx & bit_3DNOWP)    printk("  - 3DNow! Extensions\n");

    // --- Structured extended features (leaf 7) ---
    cpuid_leaf(7, &info);

    printk("[Leaf 7 EBX features]:\n");
    if (info.ebx & bit_FSGSBASE)  printk("  - FSGSBASE\n");
    if (info.ebx & bit_BMI)       printk("  - BMI1\n");
    if (info.ebx & bit_HLE)       printk("  - HLE\n");
    if (info.ebx & bit_AVX2)      printk("  - AVX2\n");
    if (info.ebx & bit_BMI2)      printk("  - BMI2\n");
    if (info.ebx & bit_RTM)       printk("  - RTM\n");
    if (info.ebx & bit_AVX512F)   printk("  - AVX-512 Foundation\n");
    if (info.ebx & bit_SHA)       printk("  - SHA extensions\n");

    printk("===========================\n");
}
