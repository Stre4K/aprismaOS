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
#include <kernel/console.h>
#include <kernel/fs.h>
#include <string.h>

#define PAGE_SIZE 4096
#define WRITE_MAX (64 * 1024)  // longest single write
#define READ_MAX  (64 * 1024)  // longest single file read

// True if user mode may read (or write) [addr, addr + len)
static bool user_buffer_ok(uint64_t addr, uint64_t len, bool write)
{
    if (len == 0)
        return true;
    if (addr + len < addr)
        return false;  // wraps around

    for (uint64_t page = addr & ~(uint64_t)(PAGE_SIZE - 1); page < addr + len; page += PAGE_SIZE) {
        if (!paging_user_access_ok(page, write))
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
    if (!user_buffer_ok(buf, len, false))
        return -EFAULT;

    printk("%.*s", (int)len, (const char *)(uintptr_t)buf);
    return (int64_t)len;
}

// The open-file slot for a file descriptor, or NULL
static open_file_t *file_slot(process_t *p, uint64_t fd)
{
    if (fd < PROCESS_FIRST_FD || fd >= PROCESS_FIRST_FD + PROCESS_MAX_FILES)
        return NULL;
    open_file_t *slot = &p->files[fd - PROCESS_FIRST_FD];
    return slot->node ? slot : NULL;
}

static int64_t sys_read(uint64_t fd, uint64_t buf, uint64_t len)
{
    if (fd == 0) {
        // Console: a line at a time
        if (len > CONSOLE_LINE_MAX)
            len = CONSOLE_LINE_MAX;
        if (!user_buffer_ok(buf, len, true))
            return -EFAULT;

        char line[CONSOLE_LINE_MAX];
        long n = console_read(line, len);
        if (n > 0)
            memcpy((void *)(uintptr_t)buf, line, (size_t)n);  // checked above
        return n;
    }

    open_file_t *file = file_slot(process_current(), fd);
    if (file == NULL)
        return -EBADF;

    size_t left = file->node->size - file->offset;
    if (len > left)
        len = left;
    if (len > READ_MAX)
        len = READ_MAX;
    if (!user_buffer_ok(buf, len, true))
        return -EFAULT;

    memcpy((void *)(uintptr_t)buf, file->node->data + file->offset, len);
    file->offset += len;
    return (int64_t)len;  // 0 at the end of the file
}

// Copy a NUL-terminated string from user mode, checking each page it
// touches. Returns its length, -EFAULT or -ENAMETOOLONG.
static int64_t copy_user_string(char *dst, uint64_t src, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        uint64_t addr = src + i;
        if (addr < src)
            return -EFAULT;  // wrapped around
        if ((i == 0 || (addr & (PAGE_SIZE - 1)) == 0) && !paging_user_access_ok(addr, false))
            return -EFAULT;
        dst[i] = *(const char *)(uintptr_t)addr;
        if (dst[i] == '\0')
            return (int64_t)i;
    }
    return -ENAMETOOLONG;
}

static int64_t sys_open(uint64_t user_path)
{
    char path[FS_PATH_MAX];
    int64_t len = copy_user_string(path, user_path, sizeof(path));
    if (len < 0)
        return len;

    const fs_node_t *node = fs_lookup(path);
    if (node == NULL)
        return -ENOENT;
    if (node->is_dir)
        return -EISDIR;

    process_t *p = process_current();
    for (int i = 0; i < PROCESS_MAX_FILES; i++) {
        if (p->files[i].node == NULL) {
            p->files[i].node = node;
            p->files[i].offset = 0;
            return PROCESS_FIRST_FD + i;
        }
    }
    return -EMFILE;
}

static int64_t sys_close(uint64_t fd)
{
    open_file_t *file = file_slot(process_current(), fd);
    if (file == NULL)
        return -EBADF;
    file->node = NULL;
    return 0;
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
        case SYS_READ:
            result = sys_read(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs), SYSCALL_ARG2(regs));
            break;
        case SYS_OPEN:
            result = sys_open(SYSCALL_ARG0(regs));
            break;
        case SYS_CLOSE:
            result = sys_close(SYSCALL_ARG0(regs));
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
