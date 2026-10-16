// arch/fpu.h (x86_64)
// x87/SSE state of user threads, saved and restored with FXSAVE/FXRSTOR.
// The kernel itself never uses these registers (-mgeneral-regs-only), so
// only threads that run user code need a save area.

#ifndef _KERNEL_ARCH_FPU_H
#define _KERNEL_ARCH_FPU_H

#define FPU_STATE_SIZE  512  // FXSAVE area
#define FPU_STATE_ALIGN 16

// Enable SSE (CR0/CR4) and record the clean state new threads start with
void arch_fpu_init(void);

// Fill a save area with the clean starting state
void arch_fpu_init_state(void *area);

// Save the current registers into / load them from a 16-byte aligned area
void arch_fpu_save(void *area);
void arch_fpu_restore(const void *area);

#endif
