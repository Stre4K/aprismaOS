// fs.h
// Read-only filesystem from the initial ramdisk: a ustar archive that the
// bootloader loads next to the kernel (a Multiboot module). Works on every
// architecture.

#ifndef __KERNEL_FS_H__
#define __KERNEL_FS_H__

#include <kernel/multiboot.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FS_PATH_MAX 256

typedef struct {
    char path[FS_PATH_MAX];  // absolute and normalized, e.g. "/bin/hello"
    const uint8_t *data;     // file contents, inside the ramdisk
    size_t size;
    bool is_dir;
} fs_node_t;

// Load the first Multiboot module as the filesystem. Returns the number of
// files and directories found, or -1 if there is no (valid) ramdisk.
int fs_init(multiboot_info_t *mbi);

bool fs_mounted(void);

// The node for an absolute path ("/" is the root directory), or NULL
const fs_node_t *fs_lookup(const char *path);

// Entries directly inside a directory: call it with index 0, 1, ... until
// it returns NULL. Returns the entry's name (not the full path).
const fs_node_t *fs_list(const char *dir, size_t index, const char **name);

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
