// ata.c
// <kernel/drivers/ata.h>
//
// The primary ATA channel's registers sit at ports 0x1F0-0x1F7 (and its
// control register at 0x3F6). A command is the sector address and count
// written into the registers, then the command byte; for each sector the
// drive raises DRQ when its 256 data words can be read from the data port.
// Interrupts are switched off on the drive (nIEN), so nothing arrives on
// IRQ 14; every wait polls the status register, with a limit so a broken
// drive cannot hang the kernel.

#include <kernel/drivers/ata.h>
#include <kernel/abi/syscall.h>
#include <kernel/arch/portio.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <stdbool.h>
#include <stddef.h>

#define ATA_DATA      0x1F0
#define ATA_ERROR     0x1F1
#define ATA_COUNT     0x1F2
#define ATA_LBA_LOW   0x1F3
#define ATA_LBA_MID   0x1F4
#define ATA_LBA_HIGH  0x1F5
#define ATA_DRIVE     0x1F6
#define ATA_STATUS    0x1F7  // read
#define ATA_COMMAND   0x1F7  // write
#define ATA_CONTROL   0x3F6  // write; reading it gives the status without side effects

#define STATUS_ERR  0x01
#define STATUS_DRQ  0x08  // data ready to move
#define STATUS_DF   0x20  // drive fault
#define STATUS_BSY  0x80
#define CONTROL_NIEN 0x02  // no interrupts

#define CMD_READ_SECTORS     0x20
#define CMD_READ_SECTORS_EXT 0x24
#define CMD_IDENTIFY         0xEC

#define POLL_LIMIT 10000000

static blockdev_t disk;
static bool lba48;

// The drive needs about 400 ns after selecting it or sending a command
// before its status means anything; four reads of the status take that long
static void delay_400ns(void)
{
    for (int i = 0; i < 4; i++)
        inb(ATA_CONTROL);
}

// Wait until the drive is not busy; with `data`, until it has data ready.
// 0, or -EIO on an error, a fault or when it takes too long.
static int wait_ready(bool data)
{
    for (long i = 0; i < POLL_LIMIT; i++) {
        uint8_t status = inb(ATA_STATUS);
        if (status & STATUS_BSY)
            continue;
        if (status & (STATUS_ERR | STATUS_DF))
            return -EIO;
        if (!data || (status & STATUS_DRQ))
            return 0;
    }
    return -EIO;
}

static void read_words(uint16_t *buf, int count)
{
    for (int i = 0; i < count; i++)
        buf[i] = inw(ATA_DATA);
}

// Read up to 256 sectors with one command
static int read_chunk(uint64_t lba, uint32_t count, uint8_t *buf)
{
    if (wait_ready(false) < 0)
        return -EIO;

    if (lba48 && lba + count > 0x0FFFFFFF) {
        outb(ATA_DRIVE, 0x40);  // master, LBA
        outb(ATA_COUNT, (uint8_t)(count >> 8));
        outb(ATA_LBA_LOW, (uint8_t)(lba >> 24));
        outb(ATA_LBA_MID, (uint8_t)(lba >> 32));
        outb(ATA_LBA_HIGH, (uint8_t)(lba >> 40));
        outb(ATA_COUNT, (uint8_t)count);
        outb(ATA_LBA_LOW, (uint8_t)lba);
        outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
        outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
        outb(ATA_COMMAND, CMD_READ_SECTORS_EXT);
    } else {
        outb(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));  // master, LBA, top 4 bits
        outb(ATA_COUNT, (uint8_t)count);  // 0 means 256
        outb(ATA_LBA_LOW, (uint8_t)lba);
        outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
        outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
        outb(ATA_COMMAND, CMD_READ_SECTORS);
    }

    for (uint32_t s = 0; s < count; s++) {
        delay_400ns();
        if (wait_ready(true) < 0)
            return -EIO;
        read_words((uint16_t *)(buf + s * SECTOR_SIZE), SECTOR_SIZE / 2);
    }
    return 0;
}

static int ata_read(blockdev_t *dev, uint64_t lba, uint32_t count, void *buf)
{
    if (lba > dev->sectors || count > dev->sectors - lba)
        return -EIO;
    int r = 0;
    preempt_disable();  // one command at a time
    for (uint32_t done = 0; r == 0 && done < count;) {
        uint32_t n = count - done < 256 ? count - done : 256;
        r = read_chunk(lba + done, n, (uint8_t *)buf + (size_t)done * SECTOR_SIZE);
        done += n;
    }
    preempt_enable();
    return r;
}

blockdev_t *ata_init(void)
{
    // No controller at all: the bus floats high
    if (inb(ATA_STATUS) == 0xFF)
        return NULL;

    outb(ATA_CONTROL, CONTROL_NIEN);
    outb(ATA_DRIVE, 0xA0);  // master
    delay_400ns();
    outb(ATA_COUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);
    outb(ATA_COMMAND, CMD_IDENTIFY);
    if (inb(ATA_STATUS) == 0)
        return NULL;  // no drive
    for (long i = 0; i < POLL_LIMIT && (inb(ATA_STATUS) & STATUS_BSY); i++)
        ;
    if (inb(ATA_LBA_MID) || inb(ATA_LBA_HIGH))
        return NULL;  // not an ATA disk (e.g. a CD drive, ATAPI)
    if (wait_ready(true) < 0)
        return NULL;

    uint16_t id[256];
    read_words(id, 256);
    if (!(id[49] & (1 << 9)))
        return NULL;  // no LBA addressing (very old disks)

    lba48 = id[83] & (1 << 10);
    uint64_t sectors = lba48 ? (uint64_t)id[100] | (uint64_t)id[101] << 16 | (uint64_t)id[102] << 32 |
                                   (uint64_t)id[103] << 48
                             : (uint64_t)id[60] | (uint64_t)id[61] << 16;
    if (sectors == 0)
        return NULL;

    // The model name: words 27-46, two characters each, high byte first
    char model[41];
    for (int i = 0; i < 20; i++) {
        model[2 * i] = (char)(id[27 + i] >> 8);
        model[2 * i + 1] = (char)id[27 + i];
    }
    int len = 40;
    while (len > 0 && model[len - 1] == ' ')
        len--;
    model[len] = '\0';

    disk = (blockdev_t){ .name = "ata0", .sectors = sectors, .read = ata_read };
    printk("[ATA] %s: %llu MiB (%s)\n", disk.name, (unsigned long long)(sectors / 2048), model);
    return &disk;
}
