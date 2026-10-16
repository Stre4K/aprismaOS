// sched.h
// Kernel threads and the preemptive round-robin scheduler
//
// Built where the architecture defines CONFIG_SCHED (i386 and x86_64).
// Elsewhere the preemption calls below are no-ops so shared code can use
// them unconditionally; a new arch needs arch/<arch>/sched (context
// switch and initial stack) and KSTACK_* in its memlayout.h.

#ifndef __KERNEL_SCHED_H__
#define __KERNEL_SCHED_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_SCHED

#define THREAD_NAME_LEN 16

typedef enum {
    THREAD_READY,     // waiting in the run queue
    THREAD_RUNNING,
    THREAD_SLEEPING,  // waiting for wake_tick
    THREAD_BLOCKED,   // waiting for thread_unblock()
    THREAD_DEAD,      // exited; its stack is freed by the next schedule()
} thread_state_t;

typedef struct thread {
    uintptr_t sp;                // saved stack pointer while switched out
    int id;
    char name[THREAD_NAME_LEN];
    thread_state_t state;
    uint64_t wake_tick;          // for THREAD_SLEEPING
    uint64_t ticks;              // timer ticks spent running
    int stack_slot;              // kernel stack slot, -1 for the boot stack
    uintptr_t kstack_top;        // top of the kernel stack (0 for the boot stack)
    uintptr_t space;             // address space, 0 = the kernel's
    struct process *process;     // user process this thread runs, or NULL
    struct thread *wait_next;    // link in a wait_queue_t while blocked on it
    void *fpu_state;             // x87/SSE save area (user threads only), or NULL
    struct thread *next;         // run queue link
    struct thread *all_next;     // list of all threads
} thread_t;

// A snapshot of one thread for listings
typedef struct {
    int id;
    char name[THREAD_NAME_LEN];
    thread_state_t state;
    uint64_t ticks;
} thread_info_t;

// Turn the running boot code into thread 0 ("main") and start the idle
// thread. Preemption starts with the next timer tick.
void sched_init(void);

// Create a kernel thread that runs entry(arg); returns NULL when out of
// memory or thread slots. The thread exits when entry returns.
thread_t *thread_create(const char *name, void (*entry)(void *), void *arg);

#ifdef CONFIG_USERMODE
// Like thread_create, but the thread runs in address space `space` on
// behalf of `process`; both are set before it can first run
thread_t *thread_create_in(const char *name, void (*entry)(void *), void *arg,
                           uintptr_t space, struct process *process);
#endif

thread_t *thread_current(void);
void thread_yield(void);
void thread_sleep(uint32_t ms);
__attribute__((noreturn)) void thread_exit(void);

// Block the current thread until thread_unblock(). Call with interrupts
// disabled (so a wakeup cannot be missed); returns with them disabled.
void thread_block(void);
void thread_unblock(thread_t *t);  // safe from interrupt handlers

// Threads waiting for the same event. A waiter re-checks its condition
// after waking, so waking more threads than needed is harmless.
typedef struct {
    thread_t *head;
} wait_queue_t;

// Block the current thread on `q`. Call with interrupts disabled, after
// checking the condition (so a wakeup cannot be missed); returns with them
// disabled.
void wait_queue_sleep(wait_queue_t *q);

// Wake every thread on `q` (safe from interrupt handlers)
void wait_queue_wake_all(wait_queue_t *q);

// Fill `out` with up to `max` threads; returns how many there are
size_t thread_list(thread_info_t *out, size_t max);
const char *thread_state_name(thread_state_t state);

// While preemption is disabled the scheduler does not switch threads; a
// switch that became due meanwhile happens at preempt_enable(). Nests.
void preempt_disable(void);
void preempt_enable(void);

// Hooks for the timer and interrupt code
void sched_tick(void);      // every timer tick, in interrupt context
void sched_irq_exit(void);  // end of every hardware interrupt

#else  // !CONFIG_SCHED

static inline void preempt_disable(void) {}
static inline void preempt_enable(void) {}

#endif // CONFIG_SCHED

#endif // __KERNEL_SCHED_H__
