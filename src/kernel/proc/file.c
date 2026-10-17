// file.c
// <kernel/file.h>
//
// Open files and pipes. A pipe is a ring buffer with one wait queue for
// both sides: readers wait for data, writers for room, and every change
// wakes all of them to re-check. It counts the open files of each end, so
// readers see the end of input once all writers are closed, and writers
// get -EPIPE once all readers are.

#include <kernel/file.h>

#ifdef CONFIG_USERMODE

#include <kernel/abi/syscall.h>
#include <kernel/arch/irq.h>
#include <kernel/console.h>
#include <kernel/mm/heap.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/sched.h>
#include <string.h>

typedef struct pipe {
    char buf[PIPE_SIZE];
    size_t start;    // index of the oldest byte
    size_t length;   // bytes in the buffer
    int readers;     // open files of each end
    int writers;
    wait_queue_t wait;
} pipe_t;

static file_t console_file = { .type = FILE_CONSOLE, .refs = 1 };  // the 1 is never dropped

file_t *file_console(void)
{
    file_get(&console_file);
    return &console_file;
}

static file_t *file_new(file_type_t type)
{
    file_t *f = kcalloc(1, sizeof(*f));
    if (f) {
        f->type = type;
        f->refs = 1;
    }
    return f;
}

int file_open_path(const char *path, int flags, file_t **out)
{
    file_t *f = file_new(FILE_NODE);
    if (f == NULL)
        return -ENOMEM;
    int r = fs_open(path, flags, &f->node);
    if (r < 0) {
        kfree(f);
        return r;
    }
    f->flags = flags;
    *out = f;
    return 0;
}

bool file_pipe(file_t **read_end, file_t **write_end)
{
    pipe_t *pipe = kcalloc(1, sizeof(*pipe));
    file_t *r = file_new(FILE_PIPE_READ);
    file_t *w = file_new(FILE_PIPE_WRITE);
    if (pipe == NULL || r == NULL || w == NULL) {
        kfree(pipe);
        kfree(r);
        kfree(w);
        return false;
    }
    pipe->readers = pipe->writers = 1;
    r->pipe = w->pipe = pipe;
    *read_end = r;
    *write_end = w;
    return true;
}

void file_get(file_t *f)
{
    unsigned long flags = irq_save();
    f->refs++;
    irq_restore(flags);
}

void file_put(file_t *f)
{
    unsigned long flags = irq_save();
    bool last = --f->refs == 0;
    pipe_t *pipe = f->pipe;
    bool free_pipe = false;
    if (last && pipe) {
        if (f->type == FILE_PIPE_READ)
            pipe->readers--;
        else
            pipe->writers--;
        free_pipe = pipe->readers == 0 && pipe->writers == 0;
    }
    irq_restore(flags);

    if (!last)
        return;
    if (f->node)
        fs_close(f->node);
    if (pipe && !free_pipe)
        wait_queue_wake_all(&pipe->wait);  // the other side sees the end (or -EPIPE)
    if (free_pipe)
        kfree(pipe);
    kfree(f);
}

static bool being_killed(void)
{
    process_t *p = process_current();
    return p && p->kill_pending;
}

static long pipe_read(pipe_t *pipe, char *buf, size_t len)
{
    if (len == 0)
        return 0;

    unsigned long flags = irq_save();
    while (pipe->length == 0) {
        if (pipe->writers == 0) {
            irq_restore(flags);
            return 0;  // end of input
        }
        if (being_killed()) {
            irq_restore(flags);
            return -EINTR;
        }
        process_sleep_on(&pipe->wait);
    }

    size_t n = len < pipe->length ? len : pipe->length;
    for (size_t i = 0; i < n; i++)
        buf[i] = pipe->buf[(pipe->start + i) % PIPE_SIZE];  // user memory, checked and mapped
    pipe->start = (pipe->start + n) % PIPE_SIZE;
    pipe->length -= n;
    irq_restore(flags);

    wait_queue_wake_all(&pipe->wait);  // writers waiting for room
    return (long)n;
}

static long pipe_write(pipe_t *pipe, const char *buf, size_t len)
{
    size_t done = 0;
    long result;

    unsigned long flags = irq_save();
    for (;;) {
        if (pipe->readers == 0) {
            result = done ? (long)done : -EPIPE;
            break;
        }
        if (done == len) {
            result = (long)done;
            break;
        }
        if (pipe->length == PIPE_SIZE) {
            if (being_killed()) {
                result = done ? (long)done : -EINTR;
                break;
            }
            process_sleep_on(&pipe->wait);
            continue;
        }

        size_t room = PIPE_SIZE - pipe->length;
        size_t n = len - done < room ? len - done : room;
        for (size_t i = 0; i < n; i++)
            pipe->buf[(pipe->start + pipe->length + i) % PIPE_SIZE] = buf[done + i];
        pipe->length += n;
        done += n;
        wait_queue_wake_all(&pipe->wait);  // readers waiting for data
    }
    irq_restore(flags);
    return result;
}

long file_read(file_t *f, void *buf, size_t len)
{
    switch (f->type) {
        case FILE_CONSOLE: {
            // A line at a time, through a kernel buffer: console_read
            // copies with interrupts disabled
            char line[CONSOLE_LINE_MAX];
            long n = console_read(line, len < sizeof(line) ? len : sizeof(line));
            if (n > 0)
                memcpy(buf, line, (size_t)n);
            return n;
        }
        case FILE_NODE: {
            if ((f->flags & O_ACCMODE) == O_WRONLY)
                return -EBADF;
            long n = fs_read(f->node, f->offset, buf, len);
            if (n > 0)
                f->offset += (size_t)n;
            return n;  // 0 at the end of the file
        }
        case FILE_PIPE_READ:
            return pipe_read(f->pipe, buf, len);
        default:
            return -EBADF;
    }
}

long file_write(file_t *f, const void *buf, size_t len)
{
    switch (f->type) {
        case FILE_CONSOLE:
            printk("%.*s", (int)len, (const char *)buf);
            return (long)len;
        case FILE_PIPE_WRITE:
            return pipe_write(f->pipe, buf, len);
        case FILE_NODE: {
            if ((f->flags & O_ACCMODE) == O_RDONLY)
                return -EBADF;
            long n = fs_write(f->node, f->flags & O_APPEND ? FS_APPEND : f->offset, buf, len);
            if (n > 0)
                f->offset = f->flags & O_APPEND ? f->node->size : f->offset + (size_t)n;
            return n;
        }
        default:
            return -EBADF;
    }
}

long file_seek(file_t *f, long offset, int whence)
{
    if (f->type != FILE_NODE)
        return -ESPIPE;
    long base;
    if (whence == SEEK_SET)
        base = 0;
    else if (whence == SEEK_CUR)
        base = (long)f->offset;
    else if (whence == SEEK_END)
        base = (long)f->node->size;
    else
        return -EINVAL;
    if ((offset < 0 && base + offset < 0) || (offset > 0 && base + offset < base))
        return -EINVAL;  // before the start, or overflowing
    f->offset = (size_t)(base + offset);
    return base + offset;
}

void file_stat(file_t *f, stat_t *st)
{
    st->size = 0;
    st->writable = 0;
    st->mtime = 0;
    switch (f->type) {
        case FILE_CONSOLE:
            st->type = STAT_CONSOLE;
            break;
        case FILE_NODE:
            st->type = f->node->is_dir ? STAT_DIR : STAT_FILE;
            st->size = f->node->size;
            st->writable = f->node->writable;
            st->mtime = f->node->mtime;
            break;
        default:
            st->type = STAT_PIPE;
            break;
    }
}

#endif // CONFIG_USERMODE
