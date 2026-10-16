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
#include <stdio.h>
#include <string.h>

#define PAGE_SIZE 4096

static int next_pid = 1;

const fs_node_t *process_find_program(const char *name)
{
    char path[FS_PATH_MAX];

    if (strchr(name, '/')) {
        strncpy(path, name, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    } else {
        snprintf(path, sizeof(path), "/bin/%s", name);
    }

    const fs_node_t *node = fs_lookup(path);
    return node && !node->is_dir ? node : NULL;
}

// First code of a process's thread: the scheduler has already loaded the
// process's address space and kernel stack
static void process_start(void *arg)
{
    process_t *p = arg;
    arch_enter_user(p->entry, p->user_sp);
}

// Map the user stack; `pages` receives the physical page of each stack
// page, top page first
static bool map_user_stack(process_t *p, uintptr_t pages[USER_STACK_PAGES])
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
        pages[i] = (uintptr_t)phys;
    }
    return true;
}

// Copy into the (not yet active) process's stack at user address `vaddr`,
// through the direct map; the stack's physical pages need not be adjacent
static void stack_write(const uintptr_t pages[USER_STACK_PAGES], uintptr_t vaddr,
                        const void *src, size_t len)
{
    const uint8_t *from = src;
    while (len > 0) {
        size_t index = (USER_STACK_TOP - 1 - vaddr) / PAGE_SIZE;  // 0 = top page
        uintptr_t page_start = USER_STACK_TOP - (index + 1) * PAGE_SIZE;
        size_t offset = vaddr - page_start;
        size_t chunk = PAGE_SIZE - offset < len ? PAGE_SIZE - offset : len;

        memcpy((uint8_t *)P2V(pages[index]) + offset, from, chunk);
        vaddr += chunk;
        from += chunk;
        len -= chunk;
    }
}

// Lay out the arguments the way the System V x86-64 ABI expects at the
// program's entry point. From the stack pointer (16-byte aligned) up:
//
//   argc, argv[0] .. argv[argc-1], NULL,  NULL (no environment),
//   0, 0 (empty auxiliary vector),  then the argument strings
static uintptr_t push_arguments(const uintptr_t pages[USER_STACK_PAGES], int argc, const char *const argv[])
{
    uintptr_t sp = USER_STACK_TOP;
    uint64_t pointers[PROCESS_MAX_ARGS];

    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        sp -= len;
        stack_write(pages, sp, argv[i], len);
        pointers[i] = sp;
    }

    size_t words = 1 + (size_t)argc + 1 + 1 + 2;
    sp = (sp - words * sizeof(uint64_t)) & ~(uintptr_t)15;

    uintptr_t at = sp;
    uint64_t value = (uint64_t)argc;
    stack_write(pages, at, &value, sizeof(value));
    at += sizeof(value);
    stack_write(pages, at, pointers, (size_t)argc * sizeof(uint64_t));
    at += (size_t)argc * sizeof(uint64_t);

    uint64_t terminators[4] = { 0, 0, 0, 0 };  // argv end, envp end, AT_NULL
    stack_write(pages, at, terminators, sizeof(terminators));

    return sp;
}

static bool arguments_fit(int argc, const char *const argv[])
{
    if (argc < 1 || argc > PROCESS_MAX_ARGS)
        return false;
    size_t bytes = 0;
    for (int i = 0; i < argc; i++) {
        bytes += strlen(argv[i]) + 1;
        if (bytes > PROCESS_MAX_ARG_BYTES)
            return false;
    }
    return true;
}

process_t *process_spawn(const char *name)
{
    const char *argv[1] = { name };
    return process_spawn_args(name, 1, argv);
}

process_t *process_spawn_args(const char *name, int argc, const char *const argv[])
{
    if (!arguments_fit(argc, argv))
        return NULL;

    const fs_node_t *program = process_find_program(name);
    if (program == NULL)
        return NULL;

    process_t *p = kcalloc(1, sizeof(*p));
    if (p == NULL)
        return NULL;

    p->pid = __atomic_fetch_add(&next_pid, 1, __ATOMIC_SEQ_CST);
    const char *base = strrchr(name, '/');
    strncpy(p->name, base ? base + 1 : name, PROCESS_NAME_LEN - 1);
    p->refs = 2;

    p->space = paging_create_space();
    if (p->space == 0)
        goto fail;

    p->entry = elf_load(p->space, program->data, program->size, &p->heap_start);
    p->brk = p->heap_start;
    uintptr_t stack_pages[USER_STACK_PAGES];
    if (p->entry == 0 || !map_user_stack(p, stack_pages))
        goto fail;
    p->user_sp = push_arguments(stack_pages, argc, argv);

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

void process_add_child(process_t *parent, process_t *child)
{
    child->next_sibling = parent->children;
    parent->children = child;
}

process_t *process_take_child(process_t *parent, int pid)
{
    for (process_t **link = &parent->children; *link; link = &(*link)->next_sibling) {
        process_t *child = *link;
        if (child->pid == pid) {
            *link = child->next_sibling;
            child->next_sibling = NULL;
            return child;
        }
    }
    return NULL;
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

    // Children nobody waited for keep running on their own; the thread
    // reference frees each one when it ends
    while (p->children) {
        process_t *child = p->children;
        p->children = child->next_sibling;
        process_release(child);
    }

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
