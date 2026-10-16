// process.c
// <kernel/proc.h>
//
// A process is one kernel thread that drops to ring 3 in its own address
// space. Its process_t has two owners: the thread (released when the
// scheduler frees it) and the creator (released by process_wait()), so
// the exit code stays readable after the thread is gone.

#include <kernel/proc.h>

#ifdef CONFIG_USERMODE

#include <kernel/arch/irq.h>
#include <kernel/arch/memlayout.h>
#include <kernel/arch/syscall.h>
#include <kernel/mm/heap.h>
#include <kernel/mm/paging.h>
#include <kernel/mm/pmm.h>
#include <kernel/panic.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <string.h>

#define PAGE_SIZE 4096

static int next_pid = 1;

const user_program_t *user_program_find(const char *name)
{
    for (const user_program_t *p = user_programs; p->name; p++) {
        if (strcmp(p->name, name) == 0)
            return p;
    }
    return NULL;
}

// First code of a process's thread: the scheduler has already loaded the
// process's address space and kernel stack
static void process_start(void *arg)
{
    process_t *p = arg;
    arch_enter_user(p->entry, p->user_sp);
}

static bool map_user_stack(process_t *p)
{
    for (int i = 0; i < USER_STACK_PAGES; i++) {
        void *phys = pmm_alloc_page();
        if (phys == NULL)
            return false;
        memset(P2V(phys), 0, PAGE_SIZE);
        if (!paging_map_user(p->space, USER_STACK_TOP - (uintptr_t)(i + 1) * PAGE_SIZE, (uintptr_t)phys, true)) {
            pmm_free_page(phys);
            return false;
        }
    }
    p->user_sp = USER_STACK_TOP;
    return true;
}

process_t *process_spawn(const char *name)
{
    const user_program_t *program = user_program_find(name);
    if (program == NULL)
        return NULL;

    process_t *p = kcalloc(1, sizeof(*p));
    if (p == NULL)
        return NULL;

    p->pid = __atomic_fetch_add(&next_pid, 1, __ATOMIC_SEQ_CST);
    strncpy(p->name, name, PROCESS_NAME_LEN - 1);
    p->refs = 2;

    p->space = paging_create_space();
    if (p->space == 0)
        goto fail;

    p->entry = elf_load(p->space, program->start, (size_t)(program->end - program->start));
    if (p->entry == 0 || !map_user_stack(p))
        goto fail;

    if (thread_create_in(p->name, process_start, p, p->space, p) == NULL)
        goto fail;

    return p;

fail:
    if (p->space)
        paging_destroy_space(p->space);  // frees whatever was mapped
    kfree(p);
    return NULL;
}

void process_release(process_t *p)
{
    unsigned long flags = irq_save();
    bool last = --p->refs == 0;
    irq_restore(flags);

    if (last)
        kfree(p);
}

void process_get(process_t *p)
{
    unsigned long flags = irq_save();
    p->refs++;
    irq_restore(flags);
}

void process_check_kill(regs_t *regs)
{
    process_t *p = process_current();
    if (p && p->kill_pending)
        process_kill("interrupted (Ctrl+C)", regs);
}

int process_wait_info(process_t *p, bool *killed)
{
    unsigned long flags = irq_save();
    while (!p->exited) {
        p->waiter = thread_current();
        thread_block();
    }
    int code = p->exit_code;
    if (killed)
        *killed = p->killed;
    irq_restore(flags);

    process_release(p);
    return code;
}

int process_wait(process_t *p)
{
    return process_wait_info(p, NULL);
}

process_t *process_current(void)
{
    return thread_current()->process;
}

void process_exit(int code)
{
    process_t *p = process_current();
    if (p == NULL)
        panic("process_exit: not called from a process");

    disable_interrupts();
    p->exit_code = code;
    p->exited = true;
    if (p->waiter) {
        thread_t *waiter = p->waiter;
        p->waiter = NULL;
        thread_unblock(waiter);
    }

    thread_exit();  // the scheduler frees the thread and the address space
}

void process_kill(const char *reason, regs_t *regs)
{
    process_t *p = process_current();
    printk("[%d %s] killed: %s at 0x%llx\n", p->pid, p->name, reason,
           (unsigned long long)REGS_PC(regs));
    p->killed = true;
    process_exit(-1);
}

void process_thread_reaped(process_t *p)
{
    // The scheduler has switched to another address space by now
    paging_destroy_space(p->space);
    p->space = 0;
    process_release(p);
}

#endif // CONFIG_USERMODE
