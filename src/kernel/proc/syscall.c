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
    else if (file->type == FILE_NODE)
        most = file->node->size > file->offset ? file->node->size - file->offset : 0;
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

// Copy a path from user mode and make it absolute: relative paths start
// at the process's current directory
static int64_t user_path(uint64_t src, char out[FS_PATH_MAX])
{
    char raw[FS_PATH_MAX];
    int64_t len = copy_user_string(raw, src, sizeof(raw));
    if (len < 0)
        return len;
    return fs_path_join(process_current()->cwd, raw, out);
}

static int64_t sys_open(uint64_t user_path_arg, uint64_t flags)
{
    char path[FS_PATH_MAX];
    int64_t len = user_path(user_path_arg, path);
    if (len < 0)
        return len;

    process_t *p = process_current();
    int fd = free_fd(p, -1);
    if (fd < 0)
        return -EMFILE;
    file_t *file;
    int r = file_open_path(path, (int)flags, &file);
    if (r < 0)
        return r;
    p->fds[fd] = file;
    return fd;
}

// The calls that only take a path
static int64_t path_call(uint64_t user_path_arg, int (*call)(const char *path))
{
    char path[FS_PATH_MAX];
    int64_t len = user_path(user_path_arg, path);
    return len < 0 ? len : call(path);
}

static int make_dir(const char *path)
{
    return fs_create(path, true, NULL);
}

static int remove_file(const char *path)
{
    return fs_remove(path, false);
}

static int remove_dir(const char *path)
{
    return fs_remove(path, true);
}

static void node_stat(const fs_node_t *node, stat_t *st)
{
    st->type = node->is_dir ? STAT_DIR : STAT_FILE;
    st->size = node->is_dir ? 0 : node->size;
    st->writable = node->writable;
}

static int64_t sys_stat(uint64_t user_path_arg, uint64_t user_st)
{
    char path[FS_PATH_MAX];
    int64_t len = user_path(user_path_arg, path);
    if (len < 0)
        return len;
    if (!user_buffer_ok(user_st, sizeof(stat_t), true))
        return -EFAULT;

    stat_t st;
    fs_node_t *node;
    fs_lock();  // the node cannot go away while we read it
    int r = fs_resolve(path, &node);
    if (r == 0)
        node_stat(node, &st);
    fs_unlock();
    if (r == 0)
        memcpy((void *)(uintptr_t)user_st, &st, sizeof(st));  // checked above
    return r;
}

static int64_t sys_fstat(uint64_t fd, uint64_t user_st)
{
    file_t *file = fd_file(fd);
    if (file == NULL)
        return -EBADF;
    if (!user_buffer_ok(user_st, sizeof(stat_t), true))
        return -EFAULT;
    stat_t st;
    file_stat(file, &st);
    memcpy((void *)(uintptr_t)user_st, &st, sizeof(st));  // checked above
    return 0;
}

static int64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence)
{
    file_t *file = fd_file(fd);
    if (file == NULL)
        return -EBADF;
    return file_seek(file, (long)offset, (int)whence);
}

static int64_t sys_rename(uint64_t user_from, uint64_t user_to)
{
    char from[FS_PATH_MAX], to[FS_PATH_MAX];
    int64_t r = user_path(user_from, from);
    if (r == 0)
        r = user_path(user_to, to);
    return r < 0 ? r : fs_rename(from, to);
}

static int64_t sys_chdir(uint64_t user_path_arg)
{
    char path[FS_PATH_MAX];
    int64_t r = user_path(user_path_arg, path);
    if (r < 0)
        return r;
    fs_node_t *node;
    fs_lock();
    r = fs_resolve(path, &node);
    if (r == 0 && !node->is_dir)
        r = -ENOTDIR;
    fs_unlock();
    if (r == 0)
        strcpy(process_current()->cwd, path);
    return r;
}

static int64_t sys_getcwd(uint64_t user_buf, uint64_t size)
{
    const char *cwd = process_current()->cwd;
    size_t len = strlen(cwd);
    if (len + 1 > size)
        return -ERANGE;
    if (!user_buffer_ok(user_buf, len + 1, true))
        return -EFAULT;
    memcpy((void *)(uintptr_t)user_buf, cwd, len + 1);  // checked above
    return (int64_t)len;
}

static int64_t sys_dup2(uint64_t fd, uint64_t new_fd)
{
    file_t *file = fd_file(fd);
    if (file == NULL || new_fd >= PROCESS_MAX_FDS)
        return -EBADF;
    if (new_fd == fd)
        return (int64_t)new_fd;

    process_t *p = process_current();
    file_get(file);
    if (p->fds[new_fd])
        file_put(p->fds[new_fd]);
    p->fds[new_fd] = file;
    return (int64_t)new_fd;
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
    int64_t len = user_path(user_dir, dir);
    if (len < 0)
        return len;
    if (!user_buffer_ok(user_entry, sizeof(dirent_t), true))
        return -EFAULT;

    dirent_t entry;
    memset(&entry, 0, sizeof(entry));
    fs_node_t *node;
    fs_lock();  // the entry cannot go away while we copy it
    int r = fs_resolve(dir, &node);
    if (r == 0 && !node->is_dir)
        r = -ENOTDIR;
    if (r == 0) {
        node = fs_child(node, (size_t)index);
        r = node != NULL;  // 0: no more entries
        if (node) {
            entry.size = node->is_dir ? 0 : node->size;
            entry.is_dir = node->is_dir;
            strncpy(entry.name, node->name, sizeof(entry.name) - 1);
        }
    }
    fs_unlock();

    if (r == 1)
        memcpy((void *)(uintptr_t)user_entry, &entry, sizeof(entry));  // checked above
    return r;
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
    char path[FS_PATH_MAX];      // as given (argv[0] if there is no argv)
    char program[FS_PATH_MAX];   // a name looked up in /bin, or an absolute path
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
    options->cwd = self->cwd;  // children start where their parent is
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
    // A path ("./prog", "/tmp/prog") is relative to the current directory
    if (strchr(req->path, '/'))
        result = fs_path_join(process_current()->cwd, req->path, req->program);
    else
        strcpy(req->program, req->path);
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

    if (process_find_program(req->program) == NULL) {
        result = -ENOENT;
        goto out;
    }
    process_t *child = process_spawn_args(req->program, (int)argc, req->argv, &options);
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

static int64_t sys_wait(uint64_t pid, uint64_t user_code, uint64_t flags)
{
    if (user_code && !user_buffer_ok(user_code, sizeof(int), true))
        return -EFAULT;
    if (flags & ~(uint64_t)WAIT_NOHANG)
        return -EINVAL;

    process_t *self = process_current();
    process_t *child = process_take_child(self, (int)pid);
    if (child == NULL)
        return -ECHILD;
    if ((flags & WAIT_NOHANG) && !child->exited) {
        process_add_child(self, child);  // still running: keep it for later
        return -EAGAIN;
    }

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
            result = sys_open(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs));
            break;
        case SYS_CLOSE:
            result = sys_close(SYSCALL_ARG0(regs));
            break;
        case SYS_SPAWN:
            result = sys_spawn(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs), SYSCALL_ARG2(regs));
            break;
        case SYS_WAIT:
            result = sys_wait(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs), SYSCALL_ARG2(regs));
            break;
        case SYS_MKDIR:
            result = path_call(SYSCALL_ARG0(regs), make_dir);
            break;
        case SYS_UNLINK:
            result = path_call(SYSCALL_ARG0(regs), remove_file);
            break;
        case SYS_RMDIR:
            result = path_call(SYSCALL_ARG0(regs), remove_dir);
            break;
        case SYS_STAT:
            result = sys_stat(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs));
            break;
        case SYS_FSTAT:
            result = sys_fstat(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs));
            break;
        case SYS_LSEEK:
            result = sys_lseek(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs), SYSCALL_ARG2(regs));
            break;
        case SYS_RENAME:
            result = sys_rename(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs));
            break;
        case SYS_CHDIR:
            result = sys_chdir(SYSCALL_ARG0(regs));
            break;
        case SYS_GETCWD:
            result = sys_getcwd(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs));
            break;
        case SYS_DUP2:
            result = sys_dup2(SYSCALL_ARG0(regs), SYSCALL_ARG1(regs));
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
            // kill() and Ctrl+C wake a sleeping process early
            thread_sleep_unless((uint32_t)SYSCALL_ARG0(regs), &process_current()->kill_pending);
            result = process_current()->kill_pending ? -EINTR : 0;
            break;
        case SYS_KILL:
            result = process_kill_pid((int)SYSCALL_ARG0(regs));
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
