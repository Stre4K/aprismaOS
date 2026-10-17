// tree.c
// <kernel/fs.h>
//
// The filesystem tree, for every filesystem (the VFS). Directories keep
// their entries in a list (in the order they were made); paths are walked
// one component at a time, filling in directories of filesystems that
// load them lazily, and stepping into what is mounted on a directory.
// Contents are left to each node's filesystem (fs_ops_t).
//
// The lock just disables preemption: there is one CPU and nothing here
// sleeps or is used from interrupt handlers. Unit tests build this file
// on the host, where preempt_disable() does nothing.

#include <kernel/fs.h>
#include <kernel/abi/syscall.h>
#include <kernel/mm/heap.h>
#include <kernel/sched.h>
#include <string.h>

static fs_node_t *root;
static uint64_t (*clock_now)(void);

void fs_set_clock(uint64_t (*now)(void))
{
    clock_now = now;
}

static uint64_t now(void)
{
    return clock_now ? clock_now() : 0;
}

void fs_lock(void)
{
    preempt_disable();
}

void fs_unlock(void)
{
    preempt_enable();
}

/* ---- nodes ---- */

// A node of the parent's filesystem (memfs for a new tree's root)
static fs_node_t *new_node(fs_node_t *parent, const char *name, bool is_dir, bool writable)
{
    fs_node_t *node = kcalloc(1, sizeof(*node));
    if (node == NULL)
        return NULL;
    strcpy(node->name, name);  // callers check the length
    node->is_dir = is_dir;
    node->writable = writable;
    node->ops = parent ? parent->ops : &memfs_ops;
    node->parent = parent ? parent : node;
    node->mtime = now();

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
    if (node->ops->release)
        node->ops->release(node);
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
    if (node->mounted)
        free_tree(node->mounted);
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

// Let a filesystem fill in a directory the first time it is looked into
static int load(fs_node_t *dir)
{
    if (!dir->is_dir || dir->loaded || dir->ops->load_dir == NULL)
        return 0;
    int r = dir->ops->load_dir(dir);
    if (r == 0) {
        dir->loaded = true;
    } else {
        // Drop what it managed to add, so that a later try starts afresh
        while (dir->children) {
            fs_node_t *child = dir->children;
            dir->children = child->next;
            free_tree(child);
        }
    }
    return r;
}

static fs_node_t *find_child(fs_node_t *dir, const char *name)
{
    if (load(dir) < 0)
        return NULL;
    for (fs_node_t *child = dir->children; child; child = child->next) {
        if (strcmp(child->name, name) == 0)
            return child;
    }
    return NULL;
}

// What a directory entry stands for: the root of a filesystem mounted on it
static fs_node_t *follow(fs_node_t *node)
{
    return node->mounted ? node->mounted : node;
}

// The root of the filesystem a node belongs to: climb until the parent
// is the tree's root's own parent or belongs to another filesystem (the
// parent of a mounted root is the mount point's directory)
static fs_node_t *fs_root_of(fs_node_t *node)
{
    while (node->parent != node && node->parent->ops == node->ops)
        node = node->parent;
    return node;
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
        int r = load(node);
        if (r < 0)
            return r;
        node = find_child(node, name);
        if (node == NULL)
            return -ENOENT;
        node = follow(node);
    }

    if (last && last[0] == '\0')
        return -EINVAL;  // the root
    if (last) {
        int r = load(node);  // creating needs the directory's entries
        if (r < 0)
            return r;
    }
    *out = node;
    return 0;
}

/* ---- contents (left to the node's filesystem) ---- */

static int truncate_locked(fs_node_t *node, size_t size)
{
    if (node->is_dir)
        return -EISDIR;
    if (!node->writable || node->ops->truncate == NULL)
        return -EROFS;
    int r = node->ops->truncate(node, size);
    if (r == 0)
        node->mtime = now();
    return r;
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
    dir->mtime = node->mtime;
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

int fs_add_static(const char *path, bool is_dir, const uint8_t *data, size_t size, uint64_t mtime)
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
        if (child)
            child = follow(child);
        if (child == NULL) {
            child = new_node(node, name, final ? is_dir : true, false);
            if (child == NULL) {
                r = -ENOSPC;
                break;
            }
            child->mtime = mtime;
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
        node->parent->mtime = now();
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
    } else if (offset >= node->size || len == 0) {
        r = 0;
    } else {
        if (len > node->size - offset)
            len = node->size - offset;
        r = node->ops->read(node, offset, buf, len);
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
    } else if (!node->writable || node->ops->write == NULL) {
        r = -EROFS;
    } else if (len == 0) {
        r = 0;
    } else {
        r = node->ops->write(node, offset, buf, len);
        if (r > 0)
            node->mtime = now();
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

int fs_rename(const char *from, const char *to)
{
    fs_lock();
    fs_node_t *node, *dir;
    char name[FS_NAME_MAX];
    int r = walk(from, &node, NULL);
    if (r == 0 && node == root)
        r = -EINVAL;
    if (r == 0)
        r = walk(to, &dir, name);
    if (r == 0 && (!node->writable || !node->parent->writable || !dir->writable))
        r = -EROFS;
    if (r == 0 && fs_root_of(node) != fs_root_of(dir))
        r = -EXDEV;

    fs_node_t *existing = r == 0 ? find_child(dir, name) : NULL;
    if (r == 0 && existing == node) {
        fs_unlock();
        return 0;  // already there
    }
    if (r == 0 && node->is_dir) {
        // Not into itself or one of its own subdirectories
        for (fs_node_t *d = dir; r == 0; d = d->parent) {
            if (d == node)
                r = -EINVAL;
            if (d == root)
                break;
        }
    }
    if (r == 0 && existing && (existing->is_dir || node->is_dir))
        r = -EEXIST;

    if (r == 0) {
        if (existing) {
            unlink_node(existing);  // replaced, like fs_remove()
            if (existing->open_count > 0)
                existing->removed = true;
            else
                free_node(existing);
        }
        node->parent->mtime = dir->mtime = now();
        unlink_node(node);
        strcpy(node->name, name);
        node->parent = dir;
        fs_node_t **link = &dir->children;
        while (*link)
            link = &(*link)->next;
        *link = node;
    }
    fs_unlock();
    return r;
}

// Add one component to an absolute path being built in out[0..*len)
static int join_component(char *out, size_t *len, const char *name, size_t n)
{
    if (n == 0 || (n == 1 && name[0] == '.'))
        return 0;
    if (n == 2 && name[0] == '.' && name[1] == '.') {
        while (*len > 1 && out[*len - 1] != '/')
            (*len)--;
        if (*len > 1)
            (*len)--;  // the '/' before the removed name
        out[*len] = '\0';
        return 0;
    }
    if (n >= FS_NAME_MAX)
        return -ENAMETOOLONG;
    size_t slash = *len > 1;
    if (*len + slash + n >= FS_PATH_MAX)
        return -ENAMETOOLONG;
    if (slash)
        out[(*len)++] = '/';
    memcpy(out + *len, name, n);
    *len += n;
    out[*len] = '\0';
    return 0;
}

static int join_all(char *out, size_t *len, const char *path)
{
    while (*path) {
        while (*path == '/')
            path++;
        size_t n = 0;
        while (path[n] && path[n] != '/')
            n++;
        int r = join_component(out, len, path, n);
        if (r < 0)
            return r;
        path += n;
    }
    return 0;
}

int fs_path_join(const char *cwd, const char *path, char out[FS_PATH_MAX])
{
    size_t len = 1;
    out[0] = '/';
    out[1] = '\0';
    int r = 0;
    if (path[0] != '/')
        r = join_all(out, &len, cwd);
    if (r == 0)
        r = join_all(out, &len, path);
    return r;
}

fs_node_t *fs_child(fs_node_t *dir, size_t index)
{
    fs_lock();
    fs_node_t *child = dir->is_dir && load(dir) == 0 ? dir->children : NULL;
    while (child && index--)
        child = child->next;
    fs_unlock();
    return child;
}

/* ---- for filesystems ---- */

fs_node_t *fs_new_root(const fs_ops_t *ops, void *priv, bool writable)
{
    fs_lock();
    fs_node_t *node = new_node(NULL, "", true, writable);
    if (node) {
        node->ops = ops;
        node->priv = priv;
    }
    fs_unlock();
    return node;
}

fs_node_t *fs_new_child(fs_node_t *dir, const char *name, bool is_dir)
{
    if (strlen(name) >= FS_NAME_MAX)
        return NULL;
    return new_node(dir, name, is_dir, dir->writable);
}

int fs_mount(const char *path, fs_node_t *fs)
{
    fs_lock();
    fs_node_t *dir;
    int r = walk(path, &dir, NULL);
    if (r == 0 && !dir->is_dir)
        r = -ENOTDIR;
    if (r == 0 && (dir == root || fs_root_of(dir) != root))
        r = -EBUSY;  // only on directories of the root filesystem, once
    if (r == 0) {
        dir->mounted = fs;
        fs->parent = dir->parent;  // ".." from its root leads out of it
        strcpy(fs->name, dir->name);
    }
    fs_unlock();
    return r;
}
