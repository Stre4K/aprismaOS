// ata.h
// ATA (IDE) hard disks, through PIO: the CPU moves every word itself
// through I/O ports, polling for the disk to be ready (no DMA, no
// interrupts). Slow on real hardware, simple, and fine under QEMU.

#ifndef __KERNEL_DRIVERS_ATA_H__
#define __KERNEL_DRIVERS_ATA_H__

#include <kernel/blockdev.h>

// Look for a disk on the primary IDE channel (the master drive, QEMU's
// -hda or -drive index=0); NULL if there is none. Prints what it found.
blockdev_t *ata_init(void);

#endif // __KERNEL_DRIVERS_ATA_H__
