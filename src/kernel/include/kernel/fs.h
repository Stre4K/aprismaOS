// fs.h
// The filesystem: one tree of directories and files (the VFS). The tree
// (tree.c) knows names, paths, mounts and open files for every kind of
// filesystem; each filesystem keeps the contents behind its fs_ops_t.
//
// - memfs (memfs.c) holds files in memory. The root is one: at boot it is
//   filled from the initial ramdisk (a ustar archive the bootloader loads
//   next to the kernel, a Multiboot module), read-only and pointing into
//   the archive. /tmp is writable: files made there keep their contents in
//   a kernel heap buffer that grows as they are written.
// - Other filesystems are mounted on a directory: the root of the mounted
//   one takes its place, and ".." from there leads back out. Their
//   directories fill themselves in the first time they are used.
//
// Works on every architecture.
//
// Paths here are absolute ("/etc/motd"; one without a leading '/' starts
// at the root too). Processes have a current directory: the system calls
// turn their paths into absolute ones with fs_path_join() first. Empty
// components, "." and ".." are understood. Errors are the negative codes
// from <kernel/abi/syscall.h>.
//
// Every function here takes the filesystem lock itself. Code that reads a
// node's fields directly holds it with fs_lock() while doing so; nothing
// that can sleep may run under it (disk reads poll, they do not sleep).

#ifndef __KERNEL_FS_H__
#define __KERNEL_FS_H__

#include <kernel/multiboot.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FS_PATH_MAX  256
#define FS_NAME_MAX  128                  // one path component
#define FS_FILE_MAX  (4u * 1024 * 1024)   // largest writable memfs file
#define FS_SPACE_MAX (16u * 1024 * 1024)  // all writable memfs files together

struct fs_node;

// What a filesystem does for its nodes. The tree has already checked the
// request (a file, not a directory; writable; ...) and holds the lock.
typedef struct fs_ops {
    const char *name;  // "memfs", "fat", ...
    // Fill in a directory's entries (with fs_new_child) the first time it
    // is looked into; NULL if directories are always complete
    int (*load_dir)(struct fs_node *dir);
    // Read up to `len` bytes at `offset` (which is inside the file)
    long (*read)(struct fs_node *node, size_t offset, void *buf, size_t len);
    // Write at `offset` (maybe past the end: the gap reads as zeros);
    // NULL for a read-only filesystem
    long (*write)(struct fs_node *node, size_t offset, const void *buf, size_t len);
    int (*truncate)(struct fs_node *node, size_t size);
    // The node is being freed: free what the filesystem keeps for it
    void (*release)(struct fs_node *node);
} fs_ops_t;

typedef struct fs_node {
    char name[FS_NAME_MAX];   // "" for the root
    bool is_dir;
    bool writable;            // may change, be removed and (directories) hold new entries
    const fs_ops_t *ops;      // the filesystem it belongs to
    void *priv;               // what that filesystem keeps for it
    uint8_t *data;            // memfs: file contents
    size_t size;
    size_t capacity;          // memfs: bytes allocated for data; 0 if it points into the ramdisk
    struct fs_node *parent;   // the root is its own parent
    struct fs_node *children; // directories: entries in the order they were made
    struct fs_node *next;     // next entry of the parent
    struct fs_node *mounted;  // a mount point: the root of the filesystem mounted on it
    bool loaded;              // directories: load_dir has filled them in
    uint64_t mtime;           // last change, seconds since 1970 (UTC)
    int open_count;           // open files using it
    bool removed;             // removed while open: freed with the last close
} fs_node_t;

/* ---- boot (fs.c) ---- */

// Build the tree from the first Multiboot module and make /tmp writable.
// Returns the number of ramdisk entries, or -1 if there is no (valid)
// ramdisk; the filesystem then stays unmounted.
int fs_init(multiboot_info_t *mbi);

bool fs_mounted(void);

/* ---- the tree (tree.c) ---- */

// Start an empty tree: a read-only root (fs_init does this)
void fs_tree_init(void);

// Add a read-only ramdisk entry, making missing parent directories (with
// the same time); data stays where it is. A path that exists already is
// left alone.
int fs_add_static(const char *path, bool is_dir, const uint8_t *data, size_t size, uint64_t mtime);

// Where the time for new and changed files comes from (seconds since 1970);
// without a clock they get 0
void fs_set_clock(uint64_t (*now)(void));

// Make an existing directory writable (/tmp)
int fs_make_writable(const char *path);

// The node at `path`: 0, or -ENOENT, -ENOTDIR (a component is a file) or
// -ENAMETOOLONG. fs_lookup is the same returning NULL on errors.
int fs_resolve(const char *path, fs_node_t **out);
fs_node_t *fs_lookup(const char *path);

// Open a file for an open file (flags are the O_* open flags): creates it
// with O_CREAT, empties it with O_TRUNC, and counts the opening, so a file
// removed while open stays readable until fs_close(). -EISDIR for a
// directory, -EROFS to write or create in a read-only place, -EINVAL for
// bad flags.
int fs_open(const char *path, int flags, fs_node_t **out);
void fs_close(fs_node_t *node);

// New file or directory: -EEXIST, -ENOENT (no parent), -ENOTDIR, -EROFS
// (read-only parent), -ENOSPC (out of memory)
int fs_create(const char *path, bool is_dir, fs_node_t **out);

// Remove a file (is_dir false) or an empty directory (true): -ENOENT,
// -EROFS, -EISDIR / -ENOTDIR for the wrong kind, -ENOTEMPTY
int fs_remove(const char *path, bool is_dir);

// Read or write at `offset`; reads stop at the end of the file, writes
// grow it (filling a gap with zeros), and FS_APPEND writes at the end as
// one step. Writes: -EROFS for a read-only file, -EFBIG past FS_FILE_MAX,
// -ENOSPC when FS_SPACE_MAX or memory runs out.
#define FS_APPEND ((size_t)-1)
long fs_read(fs_node_t *node, size_t offset, void *buf, size_t len);
long fs_write(fs_node_t *node, size_t offset, const void *buf, size_t len);
int fs_truncate(fs_node_t *node, size_t size);

// Move or rename: `to` names the new place. A file may replace a file
// there (which is removed); otherwise -EEXIST. -EROFS unless both places
// are writable, -EINVAL to move a directory into itself, -EXDEV to move
// to another filesystem.
int fs_rename(const char *from, const char *to);

// The absolute, normalized form of `path` (relative to `cwd`, itself
// absolute and normalized): "//", "." and ".." are resolved by name, so
// "/tmp/a/../b" becomes "/tmp/b". 0 or -ENAMETOOLONG.
int fs_path_join(const char *cwd, const char *path, char out[FS_PATH_MAX]);

// Entry number `index` of a directory (in the order they were made), or
// NULL. A mount point is listed as itself; looking it up gives the root
// of what is mounted there.
fs_node_t *fs_child(fs_node_t *dir, size_t index);

/* ---- for filesystems ---- */

// A root for a new filesystem, to be mounted (NULL when out of memory)
fs_node_t *fs_new_root(const fs_ops_t *ops, void *priv, bool writable);

// Mount a filesystem's root on an existing directory (the mount point),
// which it then stands for: -ENOENT, -ENOTDIR, -EBUSY if something is
// mounted there already
int fs_mount(const char *path, fs_node_t *root);

// An entry for load_dir (it inherits the directory's filesystem and
// writability), or NULL when out of memory. Called with the lock held.
fs_node_t *fs_new_child(fs_node_t *dir, const char *name, bool is_dir);

/* ---- memfs (memfs.c) ---- */

extern const fs_ops_t memfs_ops;

// Bytes allocated for writable memfs files (counted against FS_SPACE_MAX)
size_t fs_space_used(void);

void fs_lock(void);
void fs_unlock(void);

/* ---- ustar parsing (no kernel dependencies, unit tested on the host) ---- */

typedef struct {
    char path[FS_PATH_MAX];  // as stored: prefix + "/" + name, maybe "./..."
    const uint8_t *data;
    size_t size;
    char type;               // '0' file, '5' directory, others are skipped
    uint64_t mtime;          // seconds since 1970
} tar_entry_t;

// Call `entry` for every member of a ustar archive. Returns the number of
// members, or -1 if the archive is malformed (bad magic or checksum, or a
// member that runs past the end). Stops early if `entry` returns false.
int tar_parse(const uint8_t *archive, size_t size,
              bool (*entry)(const tar_entry_t *e, void *ctx), void *ctx);

#endif // __KERNEL_FS_H__
