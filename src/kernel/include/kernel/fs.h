// fs.h
// The filesystem: one tree of directories and files in memory. At boot it
// is filled from the initial ramdisk (a ustar archive the bootloader loads
// next to the kernel, a Multiboot module); those nodes are read-only and
// point into the archive. /tmp is writable: files made there keep their
// contents in a kernel heap buffer that grows as they are written. Works
// on every architecture.
//
// Paths here are absolute ("/etc/motd"; one without a leading '/' starts
// at the root too). Processes have a current directory: the system calls
// turn their paths into absolute ones with fs_path_join() first. Empty
// components, "." and ".." are understood. Errors are the negative codes
// from <kernel/abi/syscall.h>.
//
// Every function here takes the filesystem lock itself. Code that reads a
// node's fields or data directly (e.g. loading a program) holds it with
// fs_lock() while doing so; nothing that can sleep may run under it.

#ifndef __KERNEL_FS_H__
#define __KERNEL_FS_H__

#include <kernel/multiboot.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FS_PATH_MAX  256
#define FS_NAME_MAX  128                  // one path component
#define FS_FILE_MAX  (4u * 1024 * 1024)   // largest writable file
#define FS_SPACE_MAX (16u * 1024 * 1024)  // all writable files together

typedef struct fs_node {
    char name[FS_NAME_MAX];   // "" for the root
    bool is_dir;
    bool writable;            // in a writable directory: may change, be removed, hold new entries
    uint8_t *data;            // file contents
    size_t size;
    size_t capacity;          // bytes allocated for data; 0 if it points into the ramdisk
    struct fs_node *parent;   // the root is its own parent
    struct fs_node *children; // directories: entries in the order they were made
    struct fs_node *next;     // next entry of the parent
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

// Add a read-only ramdisk entry, making missing parent directories; data
// stays where it is. A path that exists already is left alone.
int fs_add_static(const char *path, bool is_dir, const uint8_t *data, size_t size);

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
// are writable, -EINVAL to move a directory into itself.
int fs_rename(const char *from, const char *to);

// The absolute, normalized form of `path` (relative to `cwd`, itself
// absolute and normalized): "//", "." and ".." are resolved by name, so
// "/tmp/a/../b" becomes "/tmp/b". 0 or -ENAMETOOLONG.
int fs_path_join(const char *cwd, const char *path, char out[FS_PATH_MAX]);

// Entry number `index` of a directory (in the order they were made), or NULL
fs_node_t *fs_child(fs_node_t *dir, size_t index);

// Bytes allocated for writable files (counted against FS_SPACE_MAX)
size_t fs_space_used(void);

void fs_lock(void);
void fs_unlock(void);

/* ---- ustar parsing (no kernel dependencies, unit tested on the host) ---- */

typedef struct {
    char path[FS_PATH_MAX];  // as stored: prefix + "/" + name, maybe "./..."
    const uint8_t *data;
    size_t size;
    char type;               // '0' file, '5' directory, others are skipped
} tar_entry_t;

// Call `entry` for every member of a ustar archive. Returns the number of
// members, or -1 if the archive is malformed (bad magic or checksum, or a
// member that runs past the end). Stops early if `entry` returns false.
int tar_parse(const uint8_t *archive, size_t size,
              bool (*entry)(const tar_entry_t *e, void *ctx), void *ctx);

#endif // __KERNEL_FS_H__
