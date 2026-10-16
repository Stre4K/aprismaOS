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
#include <kernel/arch/memlayout.h>
#include <kernel/mm/heap.h>
#include <kernel/mm/pmm.h>
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

// The open file behind a descriptor of the current process, or NULL
static file_t *fd_file(uint64_t fd)
{
    if (fd >= PROCESS_MAX_FDS)
        return NULL;
    return process_current()->fds[fd];
}

// The lowest free descriptor, or -1
static int free_fd(process_t *p, int after)
{
    for (int fd = after + 1; fd < PROCESS_MAX_FDS; fd++) {
        if (p->fds[fd] == NULL)
            return fd;
    }
    return -1;
}

static int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t len)
{
    file_t *file = fd_file(fd);
    if (file == NULL)
        return -EBADF;
    if (len > WRITE_MAX)
        len = WRITE_MAX;
    if (!user_buffer_ok(buf, len, false))
        return -EFAULT;
    return file_write(file, (const void *)(uintptr_t)buf, len);
}

static int64_t sys_read(uint64_t fd, uint64_t buf, uint64_t len)
{
    file_t *file = fd_file(fd);
    if (file == NULL)
        return -EBADF;

    // Only the part of the buffer a read can fill has to be valid
    size_t most = READ_MAX;
    if (file->type == FILE_CONSOLE)
        most = CONSOLE_LINE_MAX;
    else if (file->type == FILE_PIPE_READ)
        most = PIPE_SIZE;
    else if (file->type == FILE_RAMDISK)
        most = file->node->size - file->offset;
    if (len > most)
        len = most;
    if (!user_buffer_ok(buf, len, true))
        return -EFAULT;
    return file_read(file, (void *)(uintptr_t)buf, len);
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
    int fd = free_fd(p, -1);
    if (fd < 0)
        return -EMFILE;
    p->fds[fd] = file_open_node(node);
    return p->fds[fd] ? fd : -ENOMEM;
}

static int64_t sys_close(uint64_t fd)
{
    file_t *file = fd_file(fd);
    if (file == NULL)
        return -EBADF;
    process_current()->fds[fd] = NULL;
    file_put(file);
    return 0;
}

static int64_t sys_pipe(uint64_t user_fds)
{
    if (!user_buffer_ok(user_fds, 2 * sizeof(int), true))
        return -EFAULT;

    process_t *p = process_current();
    int read_fd = free_fd(p, -1);
    int write_fd = read_fd < 0 ? -1 : free_fd(p, read_fd);
    if (write_fd < 0)
        return -EMFILE;

    file_t *read_end, *write_end;
    if (!file_pipe(&read_end, &write_end))
        return -ENOMEM;
    p->fds[read_fd] = read_end;
    p->fds[write_fd] = write_end;

    int *out = (int *)(uintptr_t)user_fds;  // checked above
    out[0] = read_fd;
    out[1] = write_fd;
    return 0;
}

static int64_t sys_readdir(uint64_t user_dir, uint64_t index, uint64_t user_entry)
{
    char dir[FS_PATH_MAX];
    int64_t len = copy_user_string(dir, user_dir, sizeof(dir));
    if (len < 0)
        return len;
    if (!user_buffer_ok(user_entry, sizeof(dirent_t), true))
        return -EFAULT;

    const fs_node_t *node = fs_lookup(dir);
    if (node == NULL)
        return -ENOENT;
    if (!node->is_dir)
        return -ENOTDIR;

    const char *name;
    node = fs_list(dir, (size_t)index, &name);
    if (node == NULL)
        return 0;  // no more entries

    dirent_t entry = { .size = node->size, .is_dir = node->is_dir };
    strncpy(entry.name, name, sizeof(entry.name) - 1);
    memcpy((void *)(uintptr_t)user_entry, &entry, sizeof(entry));  // checked above
    return 1;
}

#define PAGE_UP(x) (((x) + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1))

// Unmap and free the heap pages in [from, to)
static void heap_unmap(process_t *p, uint64_t from, uint64_t to)
{
    for (uint64_t page = from; page < to; page += PAGE_SIZE) {
        uintptr_t phys = paging_unmap_user(p->space, page);
        if (phys)
            pmm_free_page((void *)phys);
    }
}

static int64_t sys_brk(uint64_t end)
{
    process_t *p = process_current();
    if (end == 0)
        return (int64_t)p->brk;
    if (end < p->heap_start || end - p->heap_start > USER_HEAP_MAX)
        return -ENOMEM;
    if (end > USER_STACK_TOP - (USER_STACK_PAGES + 1) * PAGE_SIZE)
        return -ENOMEM;  // keep an unmapped guard page below the stack

    uint64_t mapped = PAGE_UP(p->brk);
    uint64_t wanted = PAGE_UP(end);

    // Growing: map zeroed pages; on failure undo, leaving the heap as it was
    for (uint64_t page = mapped; page < wanted; page += PAGE_SIZE) {
        void *phys = pmm_alloc_page();
        if (phys != NULL) {
            memset(P2V(phys), 0, PAGE_SIZE);
            if (paging_map_user(p->space, page, (uintptr_t)phys, true))
                continue;
            pmm_free_page(phys);
        }
        heap_unmap(p, mapped, page);
        return -ENOMEM;
    }

    if (end < p->brk) {
        // Shrinking: free whole pages, and clear the rest of the last page
        // so memory handed out again by a later brk is zeroed too
        heap_unmap(p, wanted, mapped);
        uint64_t clear_to = p->brk < wanted ? p->brk : wanted;
        memset((void *)(uintptr_t)end, 0, clear_to - end);  // mapped, this process's own
    }

    p->brk = end;
    return (int64_t)end;
}

// A spawn request copied out of user memory
typedef struct {
    char path[FS_PATH_MAX];
    char strings[PROCESS_MAX_ARG_BYTES];
    const char *argv[PROCESS_MAX_ARGS];
} spawn_request_t;

// Copy the NULL-terminated argv array and its strings; returns argc or an error
static int64_t copy_user_argv(spawn_request_t *req, uint64_t user_argv)
{
    int argc = 0;
    size_t used = 0;

    for (;;) {
        uint64_t slot = user_argv + (uint64_t)argc * sizeof(uint64_t);
        if (!user_buffer_ok(slot, sizeof(uint64_t), false))
            return -EFAULT;
        uint64_t arg = *(const uint64_t *)(uintptr_t)slot;  // checked above
        if (arg == 0)
            break;
        if (argc == PROCESS_MAX_ARGS)
            return -E2BIG;

        int64_t len = copy_user_string(req->strings + used, arg, sizeof(req->strings) - used);
        if (len == -ENAMETOOLONG)
            return -E2BIG;
        if (len < 0)
            return len;
        req->argv[argc++] = req->strings + used;
        used += (size_t)len + 1;
    }
    return argc;
}

// True if `pgid` is a group the caller may put a child in: its own, or
// that of one of its children
static bool may_join_group(process_t *self, int pgid)
{
    if (pgid == self->pgid)
        return true;
    for (process_t *child = self->children; child; child = child->next_sibling) {
        if (child->pgid == pgid)
            return true;
    }
    return false;
}

// Turn the user's spawn options into process options (no references taken)
static int64_t read_spawn_options(uint64_t user_options, process_options_t *options)
{
    process_t *self = process_current();
    spawn_options_t wanted;
    if (user_options == 0) {
        wanted = (spawn_options_t){ .fd = { 0, 1, 2 }, .pgid = 0 };  // inherit, new group
    } else {
        if (!user_buffer_ok(user_options, sizeof(wanted), false))
            return -EFAULT;
        memcpy(&wanted, (const void *)(uintptr_t)user_options, sizeof(wanted));  // checked above
    }

    for (int i = 0; i < 3; i++) {
        if (wanted.fd[i] == -1) {
            options->stdio[i] = NULL;  // not open in the child
            continue;
        }
        if (wanted.fd[i] < 0 || wanted.fd[i] >= PROCESS_MAX_FDS)
            return -EBADF;
        options->stdio[i] = self->fds[wanted.fd[i]];
        if (options->stdio[i] == NULL && user_options != 0)
            return -EBADF;  // inheriting a closed 0, 1 or 2 just leaves it closed
    }

    if (wanted.pgid != 0 && !may_join_group(self, wanted.pgid))
        return -EPERM;
    options->pgid = wanted.pgid;
    return 0;
}

static int64_t sys_spawn(uint64_t user_path, uint64_t user_argv, uint64_t user_options)
{
    spawn_request_t *req = kmalloc(sizeof(*req));  // too big for the kernel stack
    if (req == NULL)
        return -ENOEXEC;

    int64_t result = copy_user_string(req->path, user_path, sizeof(req->path));
    if (result < 0)
        goto out;

    process_options_t options;
    result = read_spawn_options(user_options, &options);
    if (result < 0)
        goto out;

    int64_t argc;
    if (user_argv == 0) {
        req->argv[0] = req->path;
        argc = 1;
    } else {
        argc = copy_user_argv(req, user_argv);
    }
    if (argc <= 0) {
        result = argc < 0 ? argc : -EINVAL;  // argv must have at least argv[0]
        goto out;
    }

    if (process_find_program(req->path) == NULL) {
        result = -ENOENT;
        goto out;
    }
    process_t *child = process_spawn_args(req->path, (int)argc, req->argv, &options);
    if (child == NULL) {
        result = -ENOEXEC;
        goto out;
    }
    process_add_child(process_current(), child);
    result = child->pid;

out:
    kfree(req);
    return result;
}

static int64_t sys_wait(uint64_t pid, uint64_t user_code)
{
    if (user_code && !user_buffer_ok(user_code, sizeof(int), true))
        return -EFAULT;

    process_t *self = process_current();
    process_t *child = process_take_child(self, (int)pid);
    if (child == NULL)
        return -ECHILD;

    // Ctrl+C goes to the child's process group while we wait, if ours had
    // the console (or nobody did)
    int previous = console_get_foreground();
    bool hand_over = previous == 0 || previous == self->pgid;
    if (hand_over)
        console_set_foreground(child->pgid);

    bool killed;
    int code = process_wait_info(child, &killed);  // releases the child

    if (hand_over)
        console_set_foreground(previous);

    if (user_code)
        *(int *)(uintptr_t)user_code = code;  // checked above
    return killed ? WAIT_KILLED : 0;
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
        case SYS_SPAWN:
            result = sys_spawn(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs), SYSCALL_ARG2(regs));
            break;
        case SYS_WAIT:
            result = sys_wait(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs));
            break;
        case SYS_PIPE:
            result = sys_pipe(SYSCALL_ARG0(regs));
            break;
        case SYS_BRK:
            result = sys_brk(SYSCALL_ARG0(regs));
            break;
        case SYS_READDIR:
            result = sys_readdir(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs), SYSCALL_ARG2(regs));
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
