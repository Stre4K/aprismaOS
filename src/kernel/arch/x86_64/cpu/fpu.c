// fpu.c (x86_64)
// <kernel/arch/fpu.h>

#include <kernel/arch/fpu.h>
#include <kernel/arch/cpu.h>
#include <stdint.h>
#include <string.h>

#define CR0_MP         (1UL << 1)   // WAIT/FWAIT honour CR0.TS
#define CR0_EM         (1UL << 2)   // emulate x87: must be off for SSE
#define CR0_TS         (1UL << 3)   // task switched: unused, eager saving
#define CR4_OSFXSR     (1UL << 9)   // OS supports FXSAVE/FXRSTOR and SSE
#define CR4_OSXMMEXCPT (1UL << 10)  // OS handles SIMD exceptions (#XM)

#define MXCSR_DEFAULT  0x1F80       // all SSE exceptions masked, round to nearest
#define FXSAVE_XMM_OFFSET 160       // XMM0-15 in the FXSAVE area
#define FXSAVE_XMM_SIZE   256

static uint8_t initial_state[FPU_STATE_SIZE] __attribute__((aligned(FPU_STATE_ALIGN)));

void arch_fpu_init(void)
{
    uint64_t cr0 = read_cr0();
    cr0 &= ~(CR0_EM | CR0_TS);
    cr0 |= CR0_MP;
    write_cr0(cr0);

    write_cr4(read_cr4() | CR4_OSFXSR | CR4_OSXMMEXCPT);

    // The clean state: x87 reset by fninit, default MXCSR, XMM registers zero
    uint32_t mxcsr = MXCSR_DEFAULT;
    __asm__ volatile("fninit; ldmxcsr %0" :: "m"(mxcsr));
    __asm__ volatile("fxsave64 %0" : "=m"(initial_state));
    memset(initial_state + FXSAVE_XMM_OFFSET, 0, FXSAVE_XMM_SIZE);
}

void arch_fpu_init_state(void *area)
{
    memcpy(area, initial_state, FPU_STATE_SIZE);
}

void arch_fpu_save(void *area)
{
    __asm__ volatile("fxsave64 (%0)" :: "r"(area) : "memory");
}

void arch_fpu_restore(const void *area)
{
    __asm__ volatile("fxrstor64 (%0)" :: "r"(area) : "memory");
}
