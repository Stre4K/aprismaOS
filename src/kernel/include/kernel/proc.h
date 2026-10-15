// proc.h
// User processes: programs running in ring 3, each in its own address space
//
// Only built where the architecture defines CONFIG_USERMODE (x86_64 so far).

#ifndef __KERNEL_PROC_H__
#define __KERNEL_PROC_H__

#ifdef CONFIG_USERMODE

#include <kernel/arch/isr.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCESS_NAME_LEN 16

struct thread;

typedef struct process {
    int pid;
    char name[PROCESS_NAME_LEN];
    uintptr_t space;           // address space (0 once freed)
    uintptr_t entry;           // program entry point
    uintptr_t user_sp;         // initial user stack pointer
    int exit_code;
    bool exited;
    bool killed;               // ended by a fault, not by exit()
    struct thread *waiter;     // thread blocked in process_wait()
    int refs;                  // its thread + the creator; freed at 0
} process_t;

// Programs built into the kernel image (src/user, see user_programs.o)
typedef struct {
    const char *name;
    const uint8_t *start;
    const uint8_t *end;
} user_program_t;

extern const user_program_t user_programs[];  // ends with a NULL name

const user_program_t *user_program_find(const char *name);

// Start a built-in program in a new process. Returns NULL for an unknown
// program or when out of memory. The caller must process_wait() for it
// (or process_release() it).
process_t *process_spawn(const char *name);

// Block until the process exits, release it and return its exit code.
// process_wait_info also says whether it was killed by a fault.
int process_wait(process_t *p);
int process_wait_info(process_t *p, bool *killed);
void process_release(process_t *p);

// The process the current thread runs, or NULL for kernel threads
process_t *process_current(void);

// End the current process
__attribute__((noreturn)) void process_exit(int code);

// End the current process after a fault in user mode
__attribute__((noreturn)) void process_kill(const char *reason, regs_t *regs);

// Called by the scheduler when a process's thread is freed
void process_thread_reaped(process_t *p);

// Load an ELF executable into a (not active) address space; returns the
// entry point, or 0 if the image is invalid or memory runs out
uintptr_t elf_load(uintptr_t space, const uint8_t *image, size_t size);

// Handle "int $0x80" from user mode
void syscall_dispatch(regs_t *regs);

#endif // CONFIG_USERMODE

#endif // __KERNEL_PROC_H__
