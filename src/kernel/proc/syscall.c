// syscall.c
// <kernel/proc.h>
//
// System calls from user mode (int $0x80). Numbers and error codes are in
// <kernel/abi/syscall.h>. Pointers from user mode are checked against the
// page tables before the kernel reads through them.

#include <kernel/proc.h>

#ifdef CONFIG_USERMODE

#include <kernel/abi/syscall.h>
#include <kernel/arch/isr.h>
#include <kernel/arch/syscall.h>
#include <kernel/mm/paging.h>
#include <kernel/printk.h>
#include <kernel/sched.h>

#define PAGE_SIZE 4096
#define WRITE_MAX (64 * 1024)  // longest single write

// True if user mode may read [addr, addr + len)
static bool user_buffer_ok(uint64_t addr, uint64_t len)
{
    if (len == 0)
        return true;
    if (addr + len < addr)
        return false;  // wraps around

    for (uint64_t page = addr & ~(uint64_t)(PAGE_SIZE - 1); page < addr + len; page += PAGE_SIZE) {
        if (!paging_user_access_ok(page, false))
            return false;
    }
    return true;
}

static int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t len)
{
    if (fd != 1 && fd != 2)
        return -EBADF;  // only stdout and stderr (the console) so far
    if (len > WRITE_MAX)
        len = WRITE_MAX;
    if (!user_buffer_ok(buf, len))
        return -EFAULT;

    printk("%.*s", (int)len, (const char *)(uintptr_t)buf);
    return (int64_t)len;
}

void syscall_dispatch(regs_t *regs)
{
    // The call arrived through an interrupt gate; let other interrupts in
    // while it runs (we are on this thread's own kernel stack)
    enable_interrupts();

    int64_t result;
    switch (SYSCALL_NUMBER(regs)) {
        case SYS_EXIT:
            process_exit((int)SYSCALL_ARG0(regs));
        case SYS_WRITE:
            result = sys_write(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs), SYSCALL_ARG2(regs));
            break;
        case SYS_GETPID:
            result = process_current()->pid;
            break;
        case SYS_SLEEP:
            thread_sleep((uint32_t)SYSCALL_ARG0(regs));
            result = 0;
            break;
        case SYS_YIELD:
            thread_yield();
            result = 0;
            break;
        default:
            result = -ENOSYS;
            break;
    }

    SYSCALL_SET_RETURN(regs, result);
    disable_interrupts();
}

#endif // CONFIG_USERMODE
