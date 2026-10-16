// file.h
// Open files: what a process's file descriptors refer to. One file can be
// behind several descriptors, also in different processes (spawn passes
// them on), so it counts its references and the last close frees it.
// Descriptors sharing a file share its position too.
//
// Only built where the architecture defines CONFIG_USERMODE.

#ifndef __KERNEL_FILE_H__
#define __KERNEL_FILE_H__

#ifdef CONFIG_USERMODE

#include <kernel/fs.h>
#include <stdbool.h>
#include <stddef.h>

#define PIPE_SIZE 4096  // bytes a pipe holds before writers wait

typedef enum {
    FILE_CONSOLE,     // keyboard input (a line at a time) and screen output
    FILE_RAMDISK,     // a file from the ramdisk, read-only
    FILE_PIPE_READ,   // the reading end of a pipe
    FILE_PIPE_WRITE,  // the writing end of a pipe
} file_type_t;

struct pipe;

typedef struct file {
    file_type_t type;
    int refs;
    const fs_node_t *node;  // FILE_RAMDISK
    size_t offset;          // FILE_RAMDISK: where the next read starts
    struct pipe *pipe;      // FILE_PIPE_*
} file_t;

// The console, shared by every process; it is never freed. Returns it
// with a reference taken.
file_t *file_console(void);

// A new open file for a ramdisk node, or NULL when out of memory
file_t *file_open_node(const fs_node_t *node);

// A new pipe: what is written to *write_end can be read from *read_end.
// False when out of memory.
bool file_pipe(file_t **read_end, file_t **write_end);

void file_get(file_t *f);  // take another reference
void file_put(file_t *f);  // drop one; the last frees the file

// Read or write at most `len` bytes; `buf` is a user buffer the caller
// has checked. Console reads return a line at a time; pipe reads wait for
// data and return 0 once every writer is gone; pipe writes wait for room
// and fail with -EPIPE once every reader is gone. Both return -EINTR if
// the process is being killed (Ctrl+C) while it waits. -EBADF if the file
// cannot be read (or written).
long file_read(file_t *f, void *buf, size_t len);
long file_write(file_t *f, const void *buf, size_t len);

#endif // CONFIG_USERMODE

#endif // __KERNEL_FILE_H__
