#ifndef __KERNEL_ARCH_CPU_H_
#define __KERNEL_ARCH_CPU_H_

#include <stdint.h>
#include <stdbool.h>

// check if the CPU supports cpuid, 0 = no, 1 = yes
uint32_t cpuid_supported_check(void);
void cpuid_detect_vendor(void);
void cpuid_detect_features(void);
void cpuid_detect_features_print(void);
void cpuid_init(void);

typedef struct {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} cpuid_info_t;

struct cpu_features_t {
    // cpuid leaf 0
    uint32_t max_basic_leaf;
    char vendor[13];
    // cpuid leaf 1
    bool apic;
    // cpuid leaf 0x80000000
    uint32_t max_extended_leaf;

};
extern struct cpu_features_t cpu_features;

/**
 * Executes the CPUID instruction with the given leaf and subleaf.
 * Stores the results in a cpuid_info_t struct.
 */
static inline void cpuid(uint32_t leaf, uint32_t subleaf, cpuid_info_t *info) {
    __asm__ volatile(
        "cpuid"
        : "=a"(info->eax), "=b"(info->ebx), "=c"(info->ecx), "=d"(info->edx)
        : "a"(leaf), "c"(subleaf)
    );
}

/**
 * Executes CPUID with just a leaf (subleaf = 0).
 */
static inline void cpuid_leaf(uint32_t leaf, cpuid_info_t *info) {
    cpuid(leaf, 0, info);
}



/* Signatures for different CPU implementations as returned in uses
   of cpuid with level 0.  */
#define signature_AMD_ebx	0x68747541
#define signature_AMD_ecx	0x444d4163
#define signature_AMD_edx	0x69746e65

#define signature_CENTAUR_ebx	0x746e6543
#define signature_CENTAUR_ecx	0x736c7561
#define signature_CENTAUR_edx	0x48727561

#define signature_CYRIX_ebx	0x69727943
#define signature_CYRIX_ecx	0x64616574
#define signature_CYRIX_edx	0x736e4978

#define signature_INTEL_ebx	0x756e6547
#define signature_INTEL_ecx	0x6c65746e
#define signature_INTEL_edx	0x49656e69

#define signature_TM1_ebx	0x6e617254
#define signature_TM1_ecx	0x55504361
#define signature_TM1_edx	0x74656d73

#define signature_TM2_ebx	0x756e6547
#define signature_TM2_ecx	0x3638784d
#define signature_TM2_edx	0x54656e69

#define signature_NSC_ebx	0x646f6547
#define signature_NSC_ecx	0x43534e20
#define signature_NSC_edx	0x79622065

#define signature_NEXGEN_ebx	0x4778654e
#define signature_NEXGEN_ecx	0x6e657669
#define signature_NEXGEN_edx	0x72446e65

#define signature_RISE_ebx	0x65736952
#define signature_RISE_ecx	0x65736952
#define signature_RISE_edx	0x65736952

#define signature_SIS_ebx	0x20536953
#define signature_SIS_ecx	0x20536953
#define signature_SIS_edx	0x20536953

#define signature_UMC_ebx	0x20434d55
#define signature_UMC_ecx	0x20434d55
#define signature_UMC_edx	0x20434d55

#define signature_VIA_ebx	0x20414956
#define signature_VIA_ecx	0x20414956
#define signature_VIA_edx	0x20414956

#define signature_VORTEX_ebx	0x74726f56
#define signature_VORTEX_ecx	0x436f5320
#define signature_VORTEX_edx	0x36387865

#define signature_SHANGHAI_ebx	0x68532020
#define signature_SHANGHAI_ecx	0x20206961
#define signature_SHANGHAI_edx	0x68676e61

typedef struct {
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    const char *name;
} cpu_vendor_t;

static const cpu_vendor_t cpu_vendors[] = {
    { signature_INTEL_ebx,    signature_INTEL_ecx,    signature_INTEL_edx,    "Intel" },
    { signature_AMD_ebx,      signature_AMD_ecx,      signature_AMD_edx,      "AMD" },
    { signature_CENTAUR_ebx,  signature_CENTAUR_ecx,  signature_CENTAUR_edx,  "Centaur" },
    { signature_CYRIX_ebx,    signature_CYRIX_ecx,    signature_CYRIX_edx,    "Cyrix" },
    { signature_TM1_ebx,      signature_TM1_ecx,      signature_TM1_edx,      "Transmeta TM1" },
    { signature_TM2_ebx,      signature_TM2_ecx,      signature_TM2_edx,      "Transmeta TM2" },
    { signature_NSC_ebx,      signature_NSC_ecx,      signature_NSC_edx,      "NSC Geode" },
    { signature_NEXGEN_ebx,   signature_NEXGEN_ecx,   signature_NEXGEN_edx,   "NexGen" },
    { signature_RISE_ebx,     signature_RISE_ecx,     signature_RISE_edx,     "Rise" },
    { signature_SIS_ebx,      signature_SIS_ecx,      signature_SIS_edx,      "SiS" },
    { signature_UMC_ebx,      signature_UMC_ecx,      signature_UMC_edx,      "UMC" },
    { signature_VIA_ebx,      signature_VIA_ecx,      signature_VIA_edx,      "VIA" },
    { signature_VORTEX_ebx,   signature_VORTEX_ecx,   signature_VORTEX_edx,   "Vortex86" },
    { signature_SHANGHAI_ebx, signature_SHANGHAI_ecx, signature_SHANGHAI_edx, "Shanghai" },
};


/* %ecx */
#define bit_SSE3	(1 << 0)
#define bit_PCLMUL	(1 << 1)
#define bit_LZCNT	(1 << 5)
#define bit_SSSE3	(1 << 9)
#define bit_FMA		(1 << 12)
#define bit_CMPXCHG16B	(1 << 13)
#define bit_SSE4_1	(1 << 19)
#define bit_SSE4_2	(1 << 20)
#define bit_MOVBE	(1 << 22)
#define bit_POPCNT	(1 << 23)
#define bit_AES		(1 << 25)
#define bit_XSAVE	(1 << 26)
#define bit_OSXSAVE	(1 << 27)
#define bit_AVX		(1 << 28)
#define bit_F16C	(1 << 29)
#define bit_RDRND	(1 << 30)

/* %edx */
#define bit_CMPXCHG8B	(1 << 8)
#define bit_APIC        (1 << 9)
#define bit_CMOV	(1 << 15)
#define bit_MMX		(1 << 23)
#define bit_FXSAVE	(1 << 24)
#define bit_SSE		(1 << 25)
#define bit_SSE2	(1 << 26)

/* Extended Features (%eax == 0x80000001) */
/* %ecx */
#define bit_LAHF_LM	(1 << 0)
#define bit_ABM		(1 << 5)
#define bit_SSE4a	(1 << 6)
#define bit_PRFCHW	(1 << 8)
#define bit_XOP         (1 << 11)
#define bit_LWP 	(1 << 15)
#define bit_FMA4        (1 << 16)
#define bit_TBM         (1 << 21)
#define bit_MWAITX      (1 << 29)

/* %edx */
#define bit_MMXEXT	(1 << 22)
#define bit_LM		(1 << 29)
#define bit_3DNOWP	(1 << 30)
#define bit_3DNOW	(1u << 31)

/* %ebx  */
#define bit_CLZERO	(1 << 0)
#define bit_WBNOINVD	(1 << 9)

/* Extended Features Leaf (%eax == 7, %ecx == 0) */
/* %ebx */
#define bit_FSGSBASE	(1 << 0)
#define bit_SGX		(1 << 2)
#define bit_BMI		(1 << 3)
#define bit_HLE		(1 << 4)
#define bit_AVX2	(1 << 5)
#define bit_BMI2	(1 << 8)
#define bit_RTM		(1 << 11)
#define bit_AVX512F	(1 << 16)
#define bit_AVX512DQ	(1 << 17)
#define bit_RDSEED	(1 << 18)
#define bit_ADX		(1 << 19)
#define bit_AVX512IFMA	(1 << 21)
#define bit_CLFLUSHOPT	(1 << 23)
#define bit_CLWB	(1 << 24)
#define bit_AVX512CD	(1 << 28)
#define bit_SHA		(1 << 29)
#define bit_AVX512BW	(1 << 30)
#define bit_AVX512VL	(1u << 31)

/* %ecx */
#define bit_AVX512VBMI	(1 << 1)
#define bit_PKU		(1 << 3)
#define bit_OSPKE	(1 << 4)
#define bit_WAITPKG	(1 << 5)
#define bit_AVX512VBMI2	(1 << 6)
#define bit_SHSTK	(1 << 7)
#define bit_GFNI	(1 << 8)
#define bit_VAES	(1 << 9)
#define bit_VPCLMULQDQ	(1 << 10)
#define bit_AVX512VNNI	(1 << 11)
#define bit_AVX512BITALG	(1 << 12)
#define bit_AVX512VPOPCNTDQ	(1 << 14)
#define bit_RDPID	(1 << 22)
#define bit_KL		(1 << 23)
#define bit_CLDEMOTE	(1 << 25)
#define bit_MOVDIRI	(1 << 27)
#define bit_MOVDIR64B	(1 << 28)
#define bit_ENQCMD	(1 << 29)

/* %edx */
#define bit_UINTR	(1 << 5)
#define bit_AVX512VP2INTERSECT	(1 << 8)
#define bit_SERIALIZE	(1 << 14)
#define bit_TSXLDTRK    (1 << 16)
#define bit_PCONFIG	(1 << 18)
#define bit_IBT         (1 << 20)
#define bit_AMX_BF16    (1 << 22)
#define bit_AVX512FP16	(1 << 23)
#define bit_AMX_TILE    (1 << 24)
#define bit_AMX_INT8    (1 << 25)

/* Extended Features Sub-leaf (%eax == 7, %ecx == 1) */
/* %eax */
#define bit_SHA512	(1 << 0)
#define bit_SM3		(1 << 1)
#define bit_SM4		(1 << 2)
#define bit_RAOINT      (1 << 3)
#define bit_AVXVNNI     (1 << 4)
#define bit_AVX512BF16  (1 << 5)
#define bit_CMPCCXADD   (1 << 7)
#define bit_AMX_COMPLEX (1 << 8)
#define bit_AMX_FP16    (1 << 21)
#define bit_HRESET      (1 << 22)
#define bit_AVXIFMA     (1 << 23)
#define bit_MOVRS	(1 << 31)

/* %edx */
#define bit_AVXVNNIINT8 (1 << 4)
#define bit_AVXNECONVERT	(1 << 5)
#define bit_AVXVNNIINT16	(1 << 10)
#define bit_PREFETCHI	(1 << 14)
#define bit_USER_MSR	(1 << 15)
#define bit_AVX10	(1 << 19)
#define bit_APX_F	(1 << 21)

/* Extended State Enumeration Sub-leaf (%eax == 0xd, %ecx == 1) */
#define bit_XSAVEOPT	(1 << 0)
#define bit_XSAVEC	(1 << 1)
#define bit_XSAVES	(1 << 3)

/* PT sub leaf (%eax == 0x14, %ecx == 0) */
/* %ebx */
#define bit_PTWRITE	(1 << 4)

/* Keylocker leaf (%eax == 0x19) */
/* %ebx */
#define bit_AESKLE	( 1<<0 )
#define bit_WIDEKL	( 1<<2 )

/* AMX sub leaf (%eax == 0x1e, %ecx == 1) */
/* %eax */
#define bit_AMX_FP8	(1 << 4)
#define bit_AMX_TRANSPOSE	(1 << 5)
#define bit_AMX_TF32	(1 << 6)
#define bit_AMX_AVX512  (1 << 7)
#define bit_AMX_MOVRS	(1 << 8)


#endif
