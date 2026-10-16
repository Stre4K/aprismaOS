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

file_t *file_open_node(const fs_node_t *node)
{
    file_t *f = file_new(FILE_RAMDISK);
    if (f)
        f->node = node;
    return f;
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
        case FILE_RAMDISK: {
            size_t left = f->node->size - f->offset;
            if (len > left)
                len = left;
            memcpy(buf, f->node->data + f->offset, len);
            f->offset += len;
            return (long)len;  // 0 at the end of the file
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
        default:
            return -EBADF;  // the ramdisk is read-only
    }
}

#endif // CONFIG_USERMODE
