// fat.h
// FAT16 and FAT32 filesystems on a block device, read-only so far. The
// filesystem may fill the whole disk ("superfloppy", what mkfs.fat makes
// of an image file) or be in the first FAT partition of an MBR partition
// table. Long names (VFAT) are read, as are the lowercase flags short
// names use; FAT times are taken as UTC.

#ifndef __KERNEL_FAT_H__
#define __KERNEL_FAT_H__

#include <kernel/blockdev.h>
#include <kernel/fs.h>

typedef struct {
    int bits;            // 16 or 32
    uint64_t bytes;      // size of the filesystem
    uint32_t cluster_bytes;
    char label[12];      // volume label, "" if there is none
} fat_info_t;

// Read the filesystem on `dev`; returns its root, read to be mounted
// with fs_mount(), or NULL (*error: -EIO, or -EINVAL if there is no FAT16
// or FAT32 filesystem). `info` (if not NULL) describes it.
fs_node_t *fat_mount(blockdev_t *dev, fat_info_t *info, int *error);

#endif // __KERNEL_FAT_H__
