// tree.c
// <kernel/fs.h>
//
// The filesystem tree. Directories keep their entries in a list (in the
// order they were made); paths are walked one component at a time. A
// writable file owns its contents, a kernel heap buffer grown on demand
// (at least doubling) and counted against FS_SPACE_MAX.
//
// The lock just disables preemption: there is one CPU and nothing here
// sleeps or is used from interrupt handlers. Unit tests build this file
// on the host, where preempt_disable() does nothing.

#include <kernel/fs.h>
#include <kernel/abi/syscall.h>
#include <kernel/mm/heap.h>
#include <kernel/sched.h>
#include <string.h>

#define MIN_CAPACITY 256

static fs_node_t *root;
static size_t space_used;

void fs_lock(void)
{
    preempt_disable();
}

void fs_unlock(void)
{
    preempt_enable();
}

/* ---- nodes ---- */

static fs_node_t *new_node(fs_node_t *parent, const char *name, bool is_dir, bool writable)
{
    fs_node_t *node = kcalloc(1, sizeof(*node));
    if (node == NULL)
        return NULL;
    strcpy(node->name, name);  // callers check the length
    node->is_dir = is_dir;
    node->writable = writable;
    node->parent = parent ? parent : node;

    if (parent) {
        fs_node_t **link = &parent->children;
        while (*link)
            link = &(*link)->next;
        *link = node;
    }
    return node;
}

static void free_node(fs_node_t *node)
{
    if (node->writable && node->data) {
        space_used -= node->capacity;
        kfree(node->data);
    }
    kfree(node);
}

static void free_tree(fs_node_t *node)
{
    fs_node_t *child = node->children;
    while (child) {
        fs_node_t *next = child->next;
        free_tree(child);
        child = next;
    }
    free_node(node);
}

static void unlink_node(fs_node_t *node)
{
    fs_node_t **link = &node->parent->children;
    while (*link != node)
        link = &(*link)->next;
    *link = node->next;
    node->next = NULL;
    node->parent = NULL;  // detached; only open files still reach it
}

static fs_node_t *find_child(fs_node_t *dir, const char *name)
{
    for (fs_node_t *child = dir->children; child; child = child->next) {
        if (strcmp(child->name, name) == 0)
            return child;
    }
    return NULL;
}

/* ---- paths ---- */

// The next component of *path; returns its length (0 at the end) or
// -ENAMETOOLONG, and moves *path past it
static int next_component(const char **path, char name[FS_NAME_MAX])
{
    const char *p = *path;
    while (*p == '/')
        p++;
    size_t n = 0;
    while (p[n] && p[n] != '/')
        n++;
    if (n >= FS_NAME_MAX)
        return -ENAMETOOLONG;
    memcpy(name, p, n);
    name[n] = '\0';
    *path = p + n;
    return (int)n;
}

static bool only_slashes(const char *p)
{
    while (*p == '/')
        p++;
    return *p == '\0';
}

// Walk `path`. With `last` set, stop before the final component: *out is
// its directory and `last` its name (-EINVAL if the path ends in the
// root, "." or "..", which cannot be made or removed).
static int walk(const char *path, fs_node_t **out, char *last)
{
    if (root == NULL)
        return -ENOENT;
    if (strlen(path) >= FS_PATH_MAX)
        return -ENAMETOOLONG;

    fs_node_t *node = root;
    char name[FS_NAME_MAX];
    if (last)
        last[0] = '\0';

    for (;;) {
        int n = next_component(&path, name);
        if (n < 0)
            return n;
        if (n == 0)
            break;
        bool dot = strcmp(name, ".") == 0, dotdot = strcmp(name, "..") == 0;
        if (!node->is_dir)
            return -ENOTDIR;
        if (last && only_slashes(path)) {
            if (dot || dotdot)
                return -EINVAL;
            strcpy(last, name);
            break;
        }
        if (dot)
            continue;
        if (dotdot) {
            node = node->parent;
            continue;
        }
        node = find_child(node, name);
        if (node == NULL)
            return -ENOENT;
    }

    if (last && last[0] == '\0')
        return -EINVAL;  // the root
    *out = node;
    return 0;
}

/* ---- contents ---- */

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

static int truncate_locked(fs_node_t *node, size_t size)
{
    if (node->is_dir)
        return -EISDIR;
    if (!node->writable)
        return -EROFS;

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

static int create_locked(const char *path, bool is_dir, fs_node_t **out)
{
    fs_node_t *dir;
    char name[FS_NAME_MAX];
    int r = walk(path, &dir, name);
    if (r < 0)
        return r == -EINVAL ? -EEXIST : r;
    if (find_child(dir, name))
        return -EEXIST;
    if (!dir->writable)
        return -EROFS;

    fs_node_t *node = new_node(dir, name, is_dir, true);
    if (node == NULL)
        return -ENOSPC;
    if (out)
        *out = node;
    return 0;
}

/* ---- the interface ---- */

void fs_tree_init(void)
{
    fs_lock();
    if (root)
        free_tree(root);
    root = new_node(NULL, "", true, false);
    fs_unlock();
}

int fs_add_static(const char *path, bool is_dir, const uint8_t *data, size_t size)
{
    int r = 0;
    char name[FS_NAME_MAX];
    fs_lock();
    fs_node_t *node = root;
    if (node == NULL)
        r = -ENOENT;
    if (r == 0 && strlen(path) >= FS_PATH_MAX)
        r = -ENAMETOOLONG;

    while (r == 0) {
        int n = next_component(&path, name);
        if (n <= 0) {
            r = n;
            break;
        }
        if (strcmp(name, ".") == 0)
            continue;
        if (strcmp(name, "..") == 0) {
            node = node->parent;
            continue;
        }
        if (!node->is_dir) {
            r = -ENOTDIR;
            break;
        }
        bool final = only_slashes(path);
        fs_node_t *child = find_child(node, name);
        if (child == NULL) {
            child = new_node(node, name, final ? is_dir : true, false);
            if (child == NULL) {
                r = -ENOSPC;
                break;
            }
            if (final && !is_dir) {
                child->data = (uint8_t *)data;  // read-only, never written or freed
                child->size = size;
            }
        }
        node = child;
    }
    fs_unlock();
    return r;
}

int fs_make_writable(const char *path)
{
    fs_lock();
    fs_node_t *node;
    int r = walk(path, &node, NULL);
    if (r == 0 && !node->is_dir)
        r = -ENOTDIR;
    if (r == 0)
        node->writable = true;
    fs_unlock();
    return r;
}

int fs_resolve(const char *path, fs_node_t **out)
{
    fs_lock();
    int r = walk(path, out, NULL);
    fs_unlock();
    return r;
}

fs_node_t *fs_lookup(const char *path)
{
    fs_node_t *node;
    return fs_resolve(path, &node) == 0 ? node : NULL;
}

int fs_open(const char *path, int flags, fs_node_t **out)
{
    int access = flags & O_ACCMODE;
    if (access == O_ACCMODE || (flags & ~(O_ACCMODE | O_CREAT | O_TRUNC | O_APPEND)))
        return -EINVAL;
    bool writing = access != O_RDONLY;

    fs_lock();
    fs_node_t *node;
    int r = walk(path, &node, NULL);
    if (r == -ENOENT && (flags & O_CREAT))
        r = create_locked(path, false, &node);
    if (r == 0 && node->is_dir)
        r = -EISDIR;
    if (r == 0 && writing && !node->writable)
        r = -EROFS;
    if (r == 0 && writing && (flags & O_TRUNC))
        r = truncate_locked(node, 0);
    if (r == 0) {
        node->open_count++;
        *out = node;
    }
    fs_unlock();
    return r;
}

void fs_close(fs_node_t *node)
{
    fs_lock();
    if (--node->open_count == 0 && node->removed)
        free_node(node);
    fs_unlock();
}

int fs_create(const char *path, bool is_dir, fs_node_t **out)
{
    fs_lock();
    int r = create_locked(path, is_dir, out);
    fs_unlock();
    return r;
}

int fs_remove(const char *path, bool is_dir)
{
    fs_lock();
    fs_node_t *node;
    int r = walk(path, &node, NULL);
    if (r == 0 && node == root)
        r = -EINVAL;
    if (r == 0 && (!node->writable || !node->parent->writable))
        r = -EROFS;
    if (r == 0 && is_dir && !node->is_dir)
        r = -ENOTDIR;
    if (r == 0 && !is_dir && node->is_dir)
        r = -EISDIR;
    if (r == 0 && node->children)
        r = -ENOTEMPTY;
    if (r == 0) {
        unlink_node(node);
        if (node->open_count > 0)
            node->removed = true;  // still readable through its open files
        else
            free_node(node);
    }
    fs_unlock();
    return r;
}

long fs_read(fs_node_t *node, size_t offset, void *buf, size_t len)
{
    fs_lock();
    long r;
    if (node->is_dir) {
        r = -EISDIR;
    } else if (offset >= node->size) {
        r = 0;
    } else {
        if (len > node->size - offset)
            len = node->size - offset;
        memcpy(buf, node->data + offset, len);
        r = (long)len;
    }
    fs_unlock();
    return r;
}

long fs_write(fs_node_t *node, size_t offset, const void *buf, size_t len)
{
    fs_lock();
    long r;
    if (offset == FS_APPEND)
        offset = node->size;
    if (node->is_dir) {
        r = -EISDIR;
    } else if (!node->writable) {
        r = -EROFS;
    } else if (len == 0) {
        r = 0;
    } else if (offset >= FS_FILE_MAX) {
        r = -EFBIG;
    } else {
        if (len > FS_FILE_MAX - offset)
            len = FS_FILE_MAX - offset;  // as much as fits
        r = reserve(node, offset + len);
        if (r == 0) {
            if (offset > node->size)
                memset(node->data + node->size, 0, offset - node->size);
            memcpy(node->data + offset, buf, len);
            if (offset + len > node->size)
                node->size = offset + len;
            r = (long)len;
        }
    }
    fs_unlock();
    return r;
}

int fs_truncate(fs_node_t *node, size_t size)
{
    fs_lock();
    int r = truncate_locked(node, size);
    fs_unlock();
    return r;
}

fs_node_t *fs_child(fs_node_t *dir, size_t index)
{
    fs_lock();
    fs_node_t *child = dir->is_dir ? dir->children : NULL;
    while (child && index--)
        child = child->next;
    fs_unlock();
    return child;
}

size_t fs_space_used(void)
{
    return space_used;
}
