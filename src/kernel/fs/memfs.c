// memfs.c
// <kernel/fs.h>
//
// Files in memory, the filesystem of / (the ramdisk) and /tmp. A ramdisk
// file points into the archive and is never written. A writable file owns
// its contents: a kernel heap buffer grown on demand (at least doubling),
// counted against FS_SPACE_MAX. Its directories are always complete, so it
// needs no load_dir.

#include <kernel/fs.h>
#include <kernel/abi/syscall.h>
#include <kernel/mm/heap.h>
#include <string.h>

#define MIN_CAPACITY 256

static size_t space_used;

size_t fs_space_used(void)
{
    return space_used;
}

// Make room for `size` bytes of a writable file
static int reserve(fs_node_t *node, size_t size)
{
    if (size <= node->capacity)
        return 0;
    if (size > FS_FILE_MAX)
        return -EFBIG;

    size_t capacity = node->capacity * 2;
    if (capacity < MIN_CAPACITY)
        capacity = MIN_CAPACITY;
    if (capacity < size)
        capacity = size;
    if (capacity > FS_FILE_MAX)
        capacity = FS_FILE_MAX;
    if (space_used - node->capacity + capacity > FS_SPACE_MAX)
        capacity = size;  // no room to grow ahead: just what is needed
    if (space_used - node->capacity + capacity > FS_SPACE_MAX)
        return -ENOSPC;

    uint8_t *data = krealloc(node->data, capacity);
    if (data == NULL)
        return -ENOSPC;
    space_used += capacity - node->capacity;
    node->data = data;
    node->capacity = capacity;
    return 0;
}

static long memfs_read(fs_node_t *node, size_t offset, void *buf, size_t len)
{
    if (len > node->size - offset)
        len = node->size - offset;
    memcpy(buf, node->data + offset, len);
    return (long)len;
}

static long memfs_write(fs_node_t *node, size_t offset, const void *buf, size_t len)
{
    if (offset >= FS_FILE_MAX)
        return -EFBIG;
    if (len > FS_FILE_MAX - offset)
        len = FS_FILE_MAX - offset;  // as much as fits
    int r = reserve(node, offset + len);
    if (r < 0)
        return r;
    if (offset > node->size)
        memset(node->data + node->size, 0, offset - node->size);
    memcpy(node->data + offset, buf, len);
    if (offset + len > node->size)
        node->size = offset + len;
    return (long)len;
}

static int memfs_truncate(fs_node_t *node, size_t size)
{
    if (size == 0 && node->data) {
        // Give the memory back: emptying is how files are rewritten
        kfree(node->data);
        space_used -= node->capacity;
        node->data = NULL;
        node->capacity = 0;
    } else if (size > node->size) {
        int r = reserve(node, size);
        if (r < 0)
            return r;
        memset(node->data + node->size, 0, size - node->size);
    }
    node->size = size;
    return 0;
}

static void memfs_release(fs_node_t *node)
{
    if (node->writable && node->data) {
        space_used -= node->capacity;
        kfree(node->data);
    }
}

const fs_ops_t memfs_ops = {
    .name = "memfs",
    .read = memfs_read,
    .write = memfs_write,
    .truncate = memfs_truncate,
    .release = memfs_release,
};
