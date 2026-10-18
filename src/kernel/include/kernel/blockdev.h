// blockdev.h
// A block device: a disk, read and written in 512-byte sectors.
// Filesystems on a disk (e.g. FAT) work through this, so they do not care
// which driver is behind it; the unit tests give them one made from a file.

#ifndef __KERNEL_BLOCKDEV_H__
#define __KERNEL_BLOCKDEV_H__

#include <stdint.h>

#define SECTOR_SIZE 512

typedef struct blockdev {
    const char *name;   // e.g. "ata0"
    uint64_t sectors;   // its size
    // Read `count` sectors from `lba` on into buf: 0, or -EIO
    int (*read)(struct blockdev *dev, uint64_t lba, uint32_t count, void *buf);
    void *priv;         // for the driver
} blockdev_t;

#endif // __KERNEL_BLOCKDEV_H__
