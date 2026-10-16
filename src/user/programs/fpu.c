// fpu: x87/SSE registers must survive being switched out. Run two copies
// at once (as 'test user' does): each fills the registers with values of
// its own and gives the CPU away many times in between.
#include "ulib.h"
#include <stdint.h>

#define SWITCHES 200

// Load all 16 XMM registers, yield SWITCHES times without touching them,
// then store them again. Everything happens in one asm block, so the
// compiler cannot save the registers for us around the system calls.
static int check_xmm(uint64_t seed)
{
    uint64_t in[32], out[32];
    for (int i = 0; i < 32; i++)
        in[i] = seed * 0x9E3779B97F4A7C15ULL + (uint64_t)i;

    __asm__ volatile(
        "movdqu 0x00(%[in]), %%xmm0\n\t"  "movdqu 0x10(%[in]), %%xmm1\n\t"
        "movdqu 0x20(%[in]), %%xmm2\n\t"  "movdqu 0x30(%[in]), %%xmm3\n\t"
        "movdqu 0x40(%[in]), %%xmm4\n\t"  "movdqu 0x50(%[in]), %%xmm5\n\t"
        "movdqu 0x60(%[in]), %%xmm6\n\t"  "movdqu 0x70(%[in]), %%xmm7\n\t"
        "movdqu 0x80(%[in]), %%xmm8\n\t"  "movdqu 0x90(%[in]), %%xmm9\n\t"
        "movdqu 0xa0(%[in]), %%xmm10\n\t" "movdqu 0xb0(%[in]), %%xmm11\n\t"
        "movdqu 0xc0(%[in]), %%xmm12\n\t" "movdqu 0xd0(%[in]), %%xmm13\n\t"
        "movdqu 0xe0(%[in]), %%xmm14\n\t" "movdqu 0xf0(%[in]), %%xmm15\n\t"
        "movl %[count], %%ecx\n"
        "1:\n\t"
        "movl %[yield], %%eax\n\t"
        "int $0x80\n\t"                   // system calls preserve all but rax
        "decl %%ecx\n\t"
        "jnz 1b\n\t"
        "movdqu %%xmm0, 0x00(%[out])\n\t"  "movdqu %%xmm1, 0x10(%[out])\n\t"
        "movdqu %%xmm2, 0x20(%[out])\n\t"  "movdqu %%xmm3, 0x30(%[out])\n\t"
        "movdqu %%xmm4, 0x40(%[out])\n\t"  "movdqu %%xmm5, 0x50(%[out])\n\t"
        "movdqu %%xmm6, 0x60(%[out])\n\t"  "movdqu %%xmm7, 0x70(%[out])\n\t"
        "movdqu %%xmm8, 0x80(%[out])\n\t"  "movdqu %%xmm9, 0x90(%[out])\n\t"
        "movdqu %%xmm10, 0xa0(%[out])\n\t" "movdqu %%xmm11, 0xb0(%[out])\n\t"
        "movdqu %%xmm12, 0xc0(%[out])\n\t" "movdqu %%xmm13, 0xd0(%[out])\n\t"
        "movdqu %%xmm14, 0xe0(%[out])\n\t" "movdqu %%xmm15, 0xf0(%[out])\n\t"
        :
        : [in] "r"(in), [out] "r"(out), [count] "i"(SWITCHES), [yield] "i"(SYS_YIELD)
        : "rax", "rcx", "memory",
          "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
          "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15");

    int bad = 0;
    for (int i = 0; i < 32; i++)
        bad += in[i] != out[i];
    return bad;
}

// The SSE and x87 rounding modes (part of the saved state) must stay as set
static int check_control_words(unsigned rounding)
{
    uint32_t mxcsr = 0x1F80 | (rounding << 13), mxcsr_after;
    uint16_t fcw = (uint16_t)(0x037F | (rounding << 10)), fcw_after;

    __asm__ volatile("ldmxcsr %0; fldcw %1" :: "m"(mxcsr), "m"(fcw));
    for (int i = 0; i < SWITCHES; i++)
        yield();
    __asm__ volatile("stmxcsr %0; fnstcw %1" : "=m"(mxcsr_after), "=m"(fcw_after));

    // Back to the defaults for the computation below
    uint32_t mxcsr_default = 0x1F80;
    uint16_t fcw_default = 0x037F;
    __asm__ volatile("ldmxcsr %0; fldcw %1" :: "m"(mxcsr_default), "m"(fcw_default));

    return (mxcsr_after != mxcsr) + (fcw_after != fcw);
}

// Ordinary floating point code: the same sum with and without giving the
// CPU away in between must come out bit for bit the same
static double harmonic(int n, int yield_every)
{
    double sum = 0.0;
    for (int i = 1; i <= n; i++) {
        sum += 1.0 / i + __builtin_sqrt((double)i) * 1e-9;
        if (yield_every && i % yield_every == 0)
            yield();
    }
    return sum;
}

int main(void)
{
    long pid = getpid();
    int bad = check_xmm((uint64_t)pid);
    bad += check_control_words((unsigned)(pid % 4));

    double a = harmonic(20000, 100);
    double b = harmonic(20000, 0);
    bad += a != b;

    print(bad ? "fpu: FAILED\n" : "fpu: ok\n");
    return bad;
}
