// test_fat.c
// FAT16 and FAT32 reading: src/kernel/fs/fat.c, mounted into the tree
// (tree.c) as the kernel does, on images that make_fat_images.sh builds with
// mkfs.fat and mtools. The block device is the image in memory, so tests can
// also break it on purpose. kmalloc and friends come from test_fs.c.

#include "aprisma.h"
#include "test.h"

#include <kernel/abi/syscall.h>
#include <kernel/blockdev.h>
#include <kernel/fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FAT_TEST_DIR
#error "FAT_TEST_DIR (where make_fat_images.sh put the images) is not set"
#endif

fs_node_t *aprisma_fat_mount(blockdev_t *dev, fat_info_t *info, int *error);
void aprisma_fs_tree_init(void);
int aprisma_fs_add_static(const char *path, bool is_dir, const uint8_t *data, size_t size, uint64_t mtime);
int aprisma_fs_mount(const char *path, fs_node_t *root);
int aprisma_fs_resolve(const char *path, fs_node_t **out);
fs_node_t *aprisma_fs_lookup(const char *path);
int aprisma_fs_open(const char *path, int flags, fs_node_t **out);
void aprisma_fs_close(fs_node_t *node);
int aprisma_fs_create(const char *path, bool is_dir, fs_node_t **out);
int aprisma_fs_remove(const char *path, bool is_dir);
long aprisma_fs_read(fs_node_t *node, size_t offset, void *buf, size_t len);
long aprisma_fs_write(fs_node_t *node, size_t offset, const void *buf, size_t len);
fs_node_t *aprisma_fs_child(fs_node_t *dir, size_t index);

/* ---- an image as a block device ---- */

typedef struct {
    uint8_t *data;
    size_t size;
    uint64_t offset_sectors;  // sectors before the image (a partition table)
    int fail_reads;           // read errors to report from now on
} image_t;

static int image_read(blockdev_t *dev, uint64_t lba, uint32_t count, void *buf)
{
    image_t *img = dev->priv;
    if (img->fail_reads)
        return -EIO;
    for (uint32_t i = 0; i < count; i++, lba++) {
        uint8_t *out = (uint8_t *)buf + (size_t)i * SECTOR_SIZE;
        if (lba < img->offset_sectors) {
            memset(out, 0, SECTOR_SIZE);
            if (lba == 0) {
                // An MBR with one FAT16 partition where the image is
                out[446 + 4] = 0x06;
                uint32_t start = (uint32_t)img->offset_sectors, sectors = (uint32_t)(img->size / SECTOR_SIZE);
                memcpy(out + 446 + 8, &start, 4);
                memcpy(out + 446 + 12, &sectors, 4);
                out[510] = 0x55;
                out[511] = 0xAA;
            }
            continue;
        }
        uint64_t at = (lba - img->offset_sectors) * SECTOR_SIZE;
        if (at + SECTOR_SIZE > img->size)
            return -EIO;
        memcpy(out, img->data + at, SECTOR_SIZE);
    }
    return 0;
}

static uint8_t *load_file(const char *name, size_t *size)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", FAT_TEST_DIR, name);
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(n ? (size_t)n : 1);
    if (data && fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        data = NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return data;
}

static image_t img;
static blockdev_t dev;
static fat_info_t info;

// Mount an image at /disk in a fresh tree; returns the mount's error
static int mount_image(const char *name, uint64_t offset_sectors)
{
    free(img.data);
    memset(&img, 0, sizeof(img));
    img.data = load_file(name, &img.size);
    img.offset_sectors = offset_sectors;
    dev = (blockdev_t){ .name = "test", .sectors = img.size / SECTOR_SIZE + offset_sectors,
                        .read = image_read, .priv = &img };

    aprisma_fs_tree_init();
    aprisma_fs_add_static("/disk", true, NULL, 0, 0);
    if (img.data == NULL)
        return -ENOENT;
    int error;
    fs_node_t *root = aprisma_fat_mount(&dev, &info, &error);
    if (root == NULL)
        return error;
    return aprisma_fs_mount("/disk", root);
}

// The whole file at `path` through fs_read, in odd-sized pieces
static uint8_t *read_all(const char *path, size_t *size, long *error)
{
    fs_node_t *node;
    *error = aprisma_fs_open(path, O_RDONLY, &node);
    if (*error < 0)
        return NULL;
    uint8_t *data = malloc(node->size + 1);
    size_t got = 0;
    long n;
    while ((n = aprisma_fs_read(node, got, data + got, 777)) > 0)
        got += (size_t)n;
    aprisma_fs_close(node);
    *error = n;
    *size = got;
    return data;
}

// The file read through FAT equals the one on the host
static int same_as_source(const char *name)
{
    char disk_path[300], src_path[300];
    snprintf(disk_path, sizeof(disk_path), "/disk/%s", name);
    snprintf(src_path, sizeof(src_path), "src/%s", name);
    size_t got_size, want_size;
    long error;
    uint8_t *got = read_all(disk_path, &got_size, &error);
    uint8_t *want = load_file(src_path, &want_size);
    int ok = got && want && error == 0 && got_size == want_size && memcmp(got, want, got_size) == 0;
    free(got);
    free(want);
    return ok;
}

static int listed(const char *dir, const char *name)
{
    fs_node_t *d = aprisma_fs_lookup(dir), *e;
    for (size_t i = 0; d && (e = aprisma_fs_child(d, i)) != NULL; i++) {
        if (strcmp(e->name, name) == 0)
            return 1;
    }
    return 0;
}

/* ---- tests ---- */

static void check_volume(const char *image, int bits)
{
    CHECK_INT_EQ(mount_image(image, 0), 0);
    CHECK_INT_EQ(info.bits, bits);
    CHECK_STR_EQ(info.label, "TESTDISK");

    // Names: 8.3 with lowercase flags, upper case, long and mixed-case names
    CHECK(listed("/disk", "hello.txt") && listed("/disk", "lower.txt") && listed("/disk", "UPPER.TXT"));
    CHECK(listed("/disk", "MixedCase.Txt"));
    CHECK(listed("/disk", "A long file name with many characters.txt"));  // four long-name entries
    CHECK(listed("/disk", "sub") && listed("/disk/sub", "deeper") && listed("/disk/sub/deeper", "note.txt"));
    CHECK(!listed("/disk", "a.txt") && !listed("/disk", "Removed with a long name.txt"));  // deleted
    CHECK(!listed("/disk", "TESTDISK") && !listed("/disk/sub", ".") && !listed("/disk/sub", ".."));
    // ... and nothing else: no deleted entries, labels or dot entries
    fs_node_t *root_dir = aprisma_fs_lookup("/disk");
    size_t entries = 0;
    while (aprisma_fs_child(root_dir, entries))
        entries++;
    CHECK_INT_EQ(entries, 10);

    // Contents, including a file over many clusters and a fragmented one
    CHECK(same_as_source("hello.txt") && same_as_source("UPPER.TXT") && same_as_source("MixedCase.Txt"));
    CHECK(same_as_source("A long file name with many characters.txt"));
    CHECK(same_as_source("big.txt"));
    CHECK(same_as_source("frag.txt"));
    CHECK(same_as_source("b.txt"));
    CHECK(same_as_source("sub/deeper/note.txt"));
    CHECK(same_as_source("empty.txt"));

    // Reading backwards and at odd places (the cluster hint must not mislead)
    fs_node_t *big;
    CHECK_INT_EQ(aprisma_fs_open("/disk/big.txt", O_RDONLY, &big), 0);
    char buf[16];
    CHECK_INT_EQ(aprisma_fs_read(big, 100000, buf, 6), 6);
    CHECK_MEM_EQ(buf, "8\n1851", 6);  // the end of "18518" and the start of "18519"
    CHECK_INT_EQ(aprisma_fs_read(big, 0, buf, 2), 2);
    CHECK_MEM_EQ(buf, "1\n", 2);
    CHECK_INT_EQ(aprisma_fs_read(big, big->size - 6, buf, 16), 6);
    CHECK_MEM_EQ(buf, "40000\n", 6);
    aprisma_fs_close(big);

    // Times (stored as UTC by the image script)
    CHECK_INT_EQ(aprisma_fs_lookup("/disk/hello.txt")->mtime, 981173106);
    CHECK(aprisma_fs_lookup("/disk/sub")->is_dir && aprisma_fs_lookup("/disk/big.txt")->size == 228894);

    // Paths through the mount
    CHECK(aprisma_fs_lookup("/disk/sub/deeper/../../hello.txt") == aprisma_fs_lookup("/disk/hello.txt"));
    CHECK(aprisma_fs_lookup("/disk/..") == aprisma_fs_lookup("/"));
    CHECK(aprisma_fs_lookup("/disk/nope") == NULL);
    fs_node_t *node;
    CHECK_INT_EQ(aprisma_fs_resolve("/disk/hello.txt/x", &node), -ENOTDIR);

    // Read-only
    CHECK_INT_EQ(aprisma_fs_open("/disk/hello.txt", O_WRONLY, &node), -EROFS);
    CHECK_INT_EQ(aprisma_fs_open("/disk/new.txt", O_WRONLY | O_CREAT, &node), -EROFS);
    CHECK_INT_EQ(aprisma_fs_create("/disk/sub/x", true, &node), -EROFS);
    CHECK_INT_EQ(aprisma_fs_remove("/disk/hello.txt", false), -EROFS);
    CHECK_INT_EQ(aprisma_fs_write(aprisma_fs_lookup("/disk/hello.txt"), 0, "x", 1), -EROFS);
}

TEST(fat16_volume_reads_like_the_source)
{
    check_volume("fat16.img", 16);
}

TEST(fat32_volume_reads_like_the_source)
{
    check_volume("fat32.img", 32);
}

TEST(fat_in_an_mbr_partition)
{
    CHECK_INT_EQ(mount_image("fat16.img", 2048), 0);
    CHECK_INT_EQ(info.bits, 16);
    CHECK(same_as_source("big.txt"));
}

TEST(fat_refuses_what_is_not_fat)
{
    CHECK_INT_EQ(mount_image("fat16.img", 0), 0);
    img.data[510] = 0;  // no boot signature
    int error;
    CHECK(aprisma_fat_mount(&dev, &info, &error) == NULL);
    CHECK_INT_EQ(error, -EINVAL);

    CHECK_INT_EQ(mount_image("fat16.img", 0), 0);
    img.data[11] = 0;
    img.data[12] = 4;  // 1024-byte sectors
    CHECK(aprisma_fat_mount(&dev, &info, &error) == NULL);
    CHECK_INT_EQ(error, -EINVAL);

    CHECK_INT_EQ(mount_image("fat32.img", 0), 0);
    dev.sectors /= 2;  // the filesystem says it is bigger than the disk
    CHECK(aprisma_fat_mount(&dev, &info, &error) == NULL);
    CHECK_INT_EQ(error, -EINVAL);

    CHECK_INT_EQ(mount_image("fat32.img", 0), 0);
    img.fail_reads = 1;
    CHECK(aprisma_fat_mount(&dev, &info, &error) == NULL);
    CHECK_INT_EQ(error, -EIO);
}

// The FAT entry of `cluster`, to break it
static uint8_t *fat16_entry(uint16_t cluster)
{
    uint16_t reserved;
    memcpy(&reserved, img.data + 14, 2);
    return img.data + reserved * SECTOR_SIZE + cluster * 2;
}

static uint16_t first_cluster_of(const char *short_name)
{
    uint16_t reserved, fat_size, root_entries;
    memcpy(&reserved, img.data + 14, 2);
    memcpy(&root_entries, img.data + 17, 2);
    memcpy(&fat_size, img.data + 22, 2);
    uint8_t *root = img.data + (reserved + 2 * fat_size) * SECTOR_SIZE;
    for (int i = 0; i < root_entries; i++) {
        if (memcmp(root + i * 32, short_name, 11) == 0) {
            uint16_t c;
            memcpy(&c, root + i * 32 + 26, 2);
            return c;
        }
    }
    return 0;
}

TEST(fat_corrupt_chains_are_errors_not_hangs)
{
    CHECK_INT_EQ(mount_image("fat16.img", 0), 0);
    uint16_t first = first_cluster_of("BIG     TXT");
    CHECK(first >= 2);

    // The second cluster points back to itself: a loop
    uint8_t *second = fat16_entry(first);
    uint16_t second_cluster = (uint16_t)(second[0] | second[1] << 8);
    uint8_t *entry = fat16_entry(second_cluster);
    entry[0] = (uint8_t)second_cluster;
    entry[1] = (uint8_t)(second_cluster >> 8);
    size_t size;
    long error;
    free(read_all("/disk/big.txt", &size, &error));
    CHECK_INT_EQ(error, -EIO);
    CHECK(size > 0 && size < 228894);  // what came before the loop was read

    // A chain into a free cluster (0) or past the end of the disk
    CHECK_INT_EQ(mount_image("fat16.img", 0), 0);
    entry = fat16_entry(first);
    entry[0] = entry[1] = 0;
    free(read_all("/disk/big.txt", &size, &error));
    CHECK_INT_EQ(error, -EIO);
    CHECK_INT_EQ(mount_image("fat16.img", 0), 0);
    entry = fat16_entry(first);
    entry[0] = entry[1] = 0xF0;  // 0xF0F0: beyond the last cluster
    free(read_all("/disk/big.txt", &size, &error));
    CHECK_INT_EQ(error, -EIO);

    // A directory whose chain loops: an error, and no half-filled listing.
    // Its cluster's free entries become deleted ones, so that reading goes
    // on into the (looping) next cluster instead of stopping there.
    CHECK_INT_EQ(mount_image("fat16.img", 0), 0);
    uint16_t sub = first_cluster_of("SUB        ");
    uint16_t reserved, fat_size, root_entries;
    memcpy(&reserved, img.data + 14, 2);
    memcpy(&root_entries, img.data + 17, 2);
    memcpy(&fat_size, img.data + 22, 2);
    uint8_t spc = img.data[13];
    uint8_t *sub_data = img.data + (reserved + 2 * fat_size + root_entries * 32 / SECTOR_SIZE) * SECTOR_SIZE +
                        (size_t)(sub - 2) * spc * SECTOR_SIZE;
    for (size_t off = 0; off < (size_t)spc * SECTOR_SIZE; off += 32) {
        if (sub_data[off] == 0)
            sub_data[off] = 0xE5;
    }
    entry = fat16_entry(sub);
    entry[0] = (uint8_t)sub;
    entry[1] = (uint8_t)(sub >> 8);
    fs_node_t *node;
    CHECK_INT_EQ(aprisma_fs_resolve("/disk/sub/deeper", &node), -EIO);
    CHECK(aprisma_fs_lookup("/disk/sub")->children == NULL);  // what it read before the loop was dropped

    // A directory that cannot be read: nothing is found in it
    CHECK_INT_EQ(mount_image("fat16.img", 0), 0);
    img.fail_reads = 1;
    CHECK_INT_EQ(aprisma_fs_resolve("/disk/hello.txt", &node), -EIO);
    img.fail_reads = 0;
    CHECK(aprisma_fs_lookup("/disk/hello.txt") != NULL);  // tried again once it works
    aprisma_fs_tree_init();
}
