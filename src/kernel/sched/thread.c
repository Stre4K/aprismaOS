// thread.c
// <kernel/sched.h>
//
// Kernel threads with a preemptive round-robin scheduler (single CPU).
//
// - Every timer tick ends the running thread's time slice if another
//   thread is ready; the switch happens on the way out of the interrupt,
//   after the interrupt has been acknowledged.
// - The idle thread runs only when no other thread is ready.
// - The scheduler's data is shared with interrupt handlers, so it is only
//   touched with interrupts disabled.
// - A thread that exits cannot free the stack it is running on; its stack
//   and thread_t are freed by the next schedule() in another thread.

#include <kernel/sched.h>

#ifdef CONFIG_SCHED

#include <kernel/arch/context.h>
#include <kernel/arch/irq.h>
#include <kernel/arch/isr.h>
#include <kernel/arch/memlayout.h>
#include <kernel/arch/timer.h>
#include <kernel/mm/heap.h>
#include <kernel/mm/paging.h>
#include <kernel/mm/pmm.h>
#include <kernel/panic.h>
#ifdef CONFIG_USERMODE
#include <kernel/arch/syscall.h>
#include <kernel/proc.h>
#endif
#include <string.h>

#define PAGE_SIZE 4096

static thread_t main_thread;          // the boot code, on the boot stack
static thread_t *current;
static thread_t *idle_thread;
static thread_t *run_head, *run_tail;  // FIFO of THREAD_READY threads (idle excluded)
static thread_t *all_threads;
static int next_id;
static bool running;                   // set once sched_init() is done

static bool stack_slot_used[KSTACK_SLOTS];

static volatile int preempt_count;
static volatile bool need_resched;

/* ---- run queue ---- */

static void run_queue_push(thread_t *t)
{
    t->next = NULL;
    if (run_tail)
        run_tail->next = t;
    else
        run_head = t;
    run_tail = t;
}

static thread_t *run_queue_pop(void)
{
    thread_t *t = run_head;
    if (t) {
        run_head = t->next;
        if (!run_head)
            run_tail = NULL;
        t->next = NULL;
    }
    return t;
}

/* ---- kernel stacks ---- */

static uintptr_t stack_top(int slot)
{
    return KSTACK_START + (uintptr_t)(slot + 1) * KSTACK_SLOT_SIZE;
}

static void stack_unmap(int slot, int pages)
{
    for (int i = 0; i < pages; i++) {
        uintptr_t phys = unmap_page(stack_top(slot) - (uintptr_t)(i + 1) * PAGE_SIZE);
        if (phys)
            pmm_free_page((void *)phys);
    }
}

// Map a stack in a free slot; the rest of the slot below it stays unmapped
// as a guard. Returns the slot, or -1. Call with interrupts disabled.
static int stack_alloc(void)
{
    int slot = 0;
    while (slot < KSTACK_SLOTS && stack_slot_used[slot])
        slot++;
    if (slot == KSTACK_SLOTS)
        return -1;

    for (int i = 0; i < KSTACK_PAGES; i++) {
        void *phys = pmm_alloc_page();
        if (phys == NULL) {
            stack_unmap(slot, i);
            return -1;
        }
        map_page(stack_top(slot) - (uintptr_t)(i + 1) * PAGE_SIZE, (uintptr_t)phys);
    }

    stack_slot_used[slot] = true;
    return slot;
}

static void stack_free(int slot)
{
    stack_unmap(slot, KSTACK_PAGES);
    stack_slot_used[slot] = false;
}

/* ---- scheduling ---- */

// Free threads that have exited. Never the current one: it may be running
// on the stack that would be freed.
static void reap_dead_threads(void)
{
    thread_t **link = &all_threads;
    while (*link) {
        thread_t *t = *link;
        if (t->state == THREAD_DEAD && t != current) {
            *link = t->all_next;
#ifdef CONFIG_USERMODE
            if (t->process)
                process_thread_reaped(t->process);  // frees its address space
#endif
            stack_free(t->stack_slot);
            kfree(t);
        } else {
            link = &t->all_next;
        }
    }
}

// Switch to the next ready thread (or idle). Call with interrupts disabled;
// returns, with them still disabled, when this thread runs again.
static void schedule(void)
{
    thread_t *prev = current;

    if (prev->state == THREAD_RUNNING) {
        prev->state = THREAD_READY;
        if (prev != idle_thread)
            run_queue_push(prev);
    }

    thread_t *next = run_queue_pop();
    if (next == NULL)
        next = idle_thread;

    next->state = THREAD_RUNNING;
    need_resched = false;

    if (next != prev) {
        current = next;
#ifdef CONFIG_USERMODE
        // Interrupts from user mode arrive on the thread's own kernel
        // stack, and each process has its own address space
        if (next->kstack_top)
            arch_set_kernel_stack(next->kstack_top);
        paging_switch_space(next->space ? next->space : paging_kernel_space());
#endif
        arch_switch_context(&prev->sp, next->sp);
    }

    reap_dead_threads();
}

// Voluntary scheduling while preemption is disabled would leave the next
// thread unpreemptible too (the count is not per thread)
static void check_can_sleep(const char *what)
{
    if (preempt_count != 0)
        panic(what);
}

static void idle_loop(void *arg)
{
    (void)arg;
    for (;;)
        enable_interrupts_and_halt();
}

// Allocate a thread with its stack; not yet runnable
static thread_t *thread_new(const char *name, void (*entry)(void *), void *arg)
{
    thread_t *t = kcalloc(1, sizeof(*t));
    if (t == NULL)
        return NULL;

    unsigned long flags = irq_save();

    int slot = stack_alloc();
    if (slot < 0) {
        irq_restore(flags);
        kfree(t);
        return NULL;
    }

    t->stack_slot = slot;
    t->kstack_top = stack_top(slot);
    t->sp = arch_thread_stack_init(stack_top(slot), entry, arg);
    t->id = next_id++;
    strncpy(t->name, name, THREAD_NAME_LEN - 1);
    t->state = THREAD_READY;
    t->all_next = all_threads;
    all_threads = t;

    irq_restore(flags);
    return t;
}

/* ---- public API ---- */

void sched_init(void)
{
    main_thread.id = 0;
    strncpy(main_thread.name, "main", THREAD_NAME_LEN - 1);
    main_thread.state = THREAD_RUNNING;
    main_thread.stack_slot = -1;
    all_threads = &main_thread;
    current = &main_thread;
    next_id = 1;

    idle_thread = thread_new("idle", idle_loop, NULL);
    if (idle_thread == NULL)
        panic("sched_init: cannot create the idle thread");

    running = true;
}

thread_t *thread_create(const char *name, void (*entry)(void *), void *arg)
{
    thread_t *t = thread_new(name, entry, arg);
    if (t == NULL)
        return NULL;

    unsigned long flags = irq_save();
    run_queue_push(t);
    irq_restore(flags);
    return t;
}

#ifdef CONFIG_USERMODE
thread_t *thread_create_in(const char *name, void (*entry)(void *), void *arg,
                           uintptr_t space, struct process *process)
{
    thread_t *t = thread_new(name, entry, arg);
    if (t == NULL)
        return NULL;

    t->space = space;
    t->process = process;

    unsigned long flags = irq_save();
    run_queue_push(t);
    irq_restore(flags);
    return t;
}
#endif

thread_t *thread_current(void)
{
    return current;
}

void thread_yield(void)
{
    check_can_sleep("thread_yield with preemption disabled");
    unsigned long flags = irq_save();
    schedule();
    irq_restore(flags);
}

void thread_sleep(uint32_t ms)
{
    check_can_sleep("thread_sleep with preemption disabled");

    uint64_t ticks = ((uint64_t)ms * PIT_FREQUENCY + 999) / 1000;
    if (ticks == 0) {
        thread_yield();
        return;
    }

    unsigned long flags = irq_save();
    current->state = THREAD_SLEEPING;
    current->wake_tick = pit_get_ticks() + ticks;
    schedule();
    irq_restore(flags);
}

void thread_exit(void)
{
    if (current == &main_thread)
        panic("thread_exit: the main thread cannot exit");

    disable_interrupts();
    current->state = THREAD_DEAD;
    schedule();  // another thread frees this one
    panic("thread_exit: a dead thread was scheduled");
}

void thread_block(void)
{
    check_can_sleep("thread_block with preemption disabled");
    current->state = THREAD_BLOCKED;
    schedule();
}

void thread_unblock(thread_t *t)
{
    unsigned long flags = irq_save();
    if (t->state == THREAD_BLOCKED) {
        t->state = THREAD_READY;
        run_queue_push(t);
        if (current == idle_thread)
            need_resched = true;  // nothing else to do: switch right away
    }
    irq_restore(flags);
}

void wait_queue_sleep(wait_queue_t *q)
{
    current->wait_next = q->head;
    q->head = current;
    thread_block();
}

void wait_queue_wake_all(wait_queue_t *q)
{
    unsigned long flags = irq_save();
    thread_t *t = q->head;
    q->head = NULL;
    while (t) {
        thread_t *next = t->wait_next;
        t->wait_next = NULL;
        thread_unblock(t);
        t = next;
    }
    irq_restore(flags);
}

size_t thread_list(thread_info_t *out, size_t max)
{
    size_t count = 0;
    unsigned long flags = irq_save();
    for (thread_t *t = all_threads; t; t = t->all_next) {
        if (count < max) {
            out[count].id = t->id;
            memcpy(out[count].name, t->name, THREAD_NAME_LEN);
            out[count].state = t->state;
            out[count].ticks = t->ticks;
        }
        count++;
    }
    irq_restore(flags);
    return count;
}

const char *thread_state_name(thread_state_t state)
{
    switch (state) {
        case THREAD_READY:    return "ready";
        case THREAD_RUNNING:  return "running";
        case THREAD_SLEEPING: return "sleeping";
        case THREAD_BLOCKED:  return "blocked";
        case THREAD_DEAD:     return "dead";
    }
    return "?";
}

void preempt_disable(void)
{
    preempt_count++;
    __asm__ volatile("" ::: "memory");
}

void preempt_enable(void)
{
    __asm__ volatile("" ::: "memory");
    // Switch now if a time slice ended while preemption was off, unless we
    // are inside an interrupt handler (interrupts disabled)
    if (--preempt_count == 0 && need_resched && running && irqs_enabled()) {
        unsigned long flags = irq_save();
        if (need_resched)
            schedule();
        irq_restore(flags);
    }
}

/* ---- interrupt hooks ---- */

void sched_tick(void)
{
    if (!running)
        return;

    current->ticks++;

    uint64_t now = pit_get_ticks();
    for (thread_t *t = all_threads; t; t = t->all_next) {
        if (t->state == THREAD_SLEEPING && now >= t->wake_tick) {
            t->state = THREAD_READY;
            run_queue_push(t);
        }
    }

    // Time slice over: switch if any other thread is waiting
    if (run_head)
        need_resched = true;
}

void sched_irq_exit(void)
{
    if (running && need_resched && preempt_count == 0)
        schedule();
}

#endif // CONFIG_SCHED
