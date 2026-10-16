// proc.h
// User processes: programs running in ring 3, each in its own address space
//
// Only built where the architecture defines CONFIG_USERMODE (x86_64 so far).

#ifndef __KERNEL_PROC_H__
#define __KERNEL_PROC_H__

#ifdef CONFIG_USERMODE

#include <kernel/arch/isr.h>
#include <kernel/fs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCESS_NAME_LEN 16
#define PROCESS_MAX_FILES 16  // open files per process
#define PROCESS_FIRST_FD 3    // 0, 1 and 2 are the console
#define PROCESS_MAX_ARGS 32   // arguments passed to a program, argv[0] included
#define PROCESS_MAX_ARG_BYTES 2048  // their total length with terminators

struct thread;

// A file a process has open (read-only)
typedef struct {
    const fs_node_t *node;     // NULL for a free slot
    size_t offset;             // where the next read starts
} open_file_t;

typedef struct process {
    int pid;
    char name[PROCESS_NAME_LEN];
    uintptr_t space;           // address space (0 once freed)
    uintptr_t entry;           // program entry point
    uintptr_t user_sp;         // initial user stack pointer
    int exit_code;
    bool exited;
    bool killed;               // ended by a fault or Ctrl+C, not by exit()
    volatile bool kill_pending; // Ctrl+C: kill when it next returns to user mode
    struct thread *waiter;     // thread blocked in process_wait()
    int refs;                  // its thread + the creator; freed at 0
    open_file_t files[PROCESS_MAX_FILES];  // file descriptor PROCESS_FIRST_FD + index
    struct process *children;  // started with the spawn system call, not yet waited for
    struct process *next_sibling;
} process_t;

// Start a program from the filesystem in a new process: `name` is looked
// up in /bin unless it contains a '/'. Returns NULL if there is no such
// program or memory runs out. The caller must process_wait() for it (or
// process_release() it).
process_t *process_spawn(const char *name);

// Same, passing arguments: the program's main() gets argc and argv, with
// argv[0] by convention its name. Fails (NULL) for more than
// PROCESS_MAX_ARGS arguments or PROCESS_MAX_ARG_BYTES of text.
process_t *process_spawn_args(const char *name, int argc, const char *const argv[]);

// The filesystem node of the program `name` would run, or NULL
const fs_node_t *process_find_program(const char *name);

// Block until the process exits, release it and return its exit code.
// process_wait_info also says whether it was killed by a fault.
int process_wait(process_t *p);
int process_wait_info(process_t *p, bool *killed);
void process_release(process_t *p);
void process_get(process_t *p);  // take another reference

// Children of a process (the spawn and wait system calls). Only the
// parent's own thread touches its list, which holds the creator's
// reference to each child until the parent waits for it or exits.
void process_add_child(process_t *parent, process_t *child);
process_t *process_take_child(process_t *parent, int pid);  // NULL if not a child

// Called before returning to user mode: ends the process if Ctrl+C asked to
void process_check_kill(regs_t *regs);

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
