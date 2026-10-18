// fat.c
// <kernel/fat.h>
//
// A FAT volume is: reserved sectors (the first holds the BIOS parameter
// block, BPB, which describes the rest), the file allocation table (FAT,
// usually twice), on FAT16 a fixed-size root directory, then the data area
// in clusters of a few sectors. A file or directory is a chain of
// clusters: the FAT holds, for every cluster, the number of the next one
// (or an end marker). Directories are arrays of 32-byte entries.
//
// Directories become tree nodes the first time they are looked into
// (load_dir); a node's private data is where its cluster chain starts.
// Everything runs under the filesystem lock, and the disk is polled, so
// the single cluster buffer and FAT sector cache need no lock of their own.
// Unit tests run this on images made with mkfs.fat and mtools.

#include <kernel/fat.h>
#include <kernel/abi/syscall.h>
#include <kernel/abi/time.h>
#include <kernel/mm/heap.h>
#include <string.h>

#define ENTRY_SIZE 32
#define ATTR_READ_ONLY 0x01
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_LONG_NAME 0x0F
#define NT_LOWER_BASE  0x08  // the short name's base is lowercase
#define NT_LOWER_EXT   0x10  // and its extension
#define LFN_LAST       0x40
#define LFN_CHARS      13    // characters in one long name entry
#define LFN_MAX        255

typedef struct {
    blockdev_t *dev;
    int bits;                  // 16 or 32
    uint32_t sectors_per_cluster;
    uint32_t cluster_bytes;
    uint64_t fat_lba;          // first sector of the (first) FAT
    uint64_t root_lba;         // FAT16: the root directory's sectors
    uint32_t root_sectors;
    uint64_t data_lba;         // cluster 2 starts here
    uint32_t clusters;         // number of data clusters (2 .. clusters + 1)
    uint32_t root_cluster;     // FAT32: where the root directory starts
    uint8_t *cluster_buf;      // one cluster, for reading
    uint64_t fat_cached;       // which FAT sector fat_buf holds (0: none)
    uint8_t fat_buf[SECTOR_SIZE];
} fat_fs_t;

typedef struct {
    fat_fs_t *fs;
    uint32_t first;            // first cluster; 0 for an empty file, or FAT16's root
    uint32_t hint_index;       // the last cluster read was number hint_index
    uint32_t hint_cluster;     // of the chain, cluster hint_cluster (0: none yet)
} fat_node_t;

#define END_OF_CHAIN 0xFFFFFFFFu
#define DIR_ENTRIES_MAX 65536  // FAT's own limit for one directory

// Brent's cycle detection while following a chain: remember a cluster,
// and every power of two steps a later one; a chain that comes back to
// the remembered cluster is a loop (a corrupt FAT)
typedef struct {
    uint32_t saved;
    uint32_t steps;
    uint32_t limit;
} loop_check_t;

static bool looped(loop_check_t *c, uint32_t cluster)
{
    if (cluster == c->saved)
        return true;
    if (++c->steps == c->limit) {
        c->saved = cluster;
        c->limit *= 2;
        c->steps = 0;
    }
    return false;
}

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* ---- clusters ---- */

static uint64_t cluster_lba(fat_fs_t *fs, uint32_t cluster)
{
    return fs->data_lba + (uint64_t)(cluster - 2) * fs->sectors_per_cluster;
}

// The cluster after `cluster` in its chain, or END_OF_CHAIN; -EIO for a
// bad or out-of-range entry (a corrupt filesystem)
static int next_cluster(fat_fs_t *fs, uint32_t cluster, uint32_t *next)
{
    uint64_t offset = (uint64_t)cluster * (uint64_t)(fs->bits / 8);
    uint64_t lba = fs->fat_lba + offset / SECTOR_SIZE;
    if (fs->fat_cached != lba) {
        if (fs->dev->read(fs->dev, lba, 1, fs->fat_buf) < 0) {
            fs->fat_cached = 0;
            return -EIO;
        }
        fs->fat_cached = lba;
    }
    const uint8_t *p = fs->fat_buf + offset % SECTOR_SIZE;
    uint32_t value = fs->bits == 16 ? le16(p) : le32(p) & 0x0FFFFFFF;
    uint32_t end = fs->bits == 16 ? 0xFFF8 : 0x0FFFFFF8;

    if (value >= end) {
        *next = END_OF_CHAIN;
        return 0;
    }
    if (value < 2 || value >= fs->clusters + 2)
        return -EIO;  // free, bad (0xFFF7 / 0x0FFFFFF7) or past the end
    *next = value;
    return 0;
}

static bool valid_cluster(fat_fs_t *fs, uint32_t cluster)
{
    return cluster >= 2 && cluster < fs->clusters + 2;
}

// Cluster number `index` of the chain that starts at `first`
static int find_cluster(fat_node_t *n, uint32_t index, uint32_t *out)
{
    uint32_t cluster = n->first, at = 0;
    if (n->hint_cluster && n->hint_index <= index) {
        cluster = n->hint_cluster;  // reading on from where the last read ended
        at = n->hint_index;
    }
    if (!valid_cluster(n->fs, cluster))
        return -EIO;
    loop_check_t check = { .saved = cluster, .limit = 1 };
    for (; at < index; at++) {
        int r = next_cluster(n->fs, cluster, &cluster);
        if (r < 0)
            return r;
        if (cluster == END_OF_CHAIN)
            return -EIO;  // shorter than the file's size says
        if (looped(&check, cluster))
            return -EIO;
    }
    n->hint_index = index;
    n->hint_cluster = cluster;
    *out = cluster;
    return 0;
}

/* ---- files ---- */

static long fat_read(fs_node_t *node, size_t offset, void *buf, size_t len)
{
    fat_node_t *n = node->priv;
    fat_fs_t *fs = n->fs;
    size_t done = 0;
    while (done < len) {
        uint32_t cluster;
        size_t at = offset + done;
        int r = find_cluster(n, (uint32_t)(at / fs->cluster_bytes), &cluster);
        if (r == 0)
            r = fs->dev->read(fs->dev, cluster_lba(fs, cluster), fs->sectors_per_cluster, fs->cluster_buf);
        if (r < 0)
            return done ? (long)done : r;
        size_t in_cluster = at % fs->cluster_bytes;
        size_t n_bytes = fs->cluster_bytes - in_cluster;
        if (n_bytes > len - done)
            n_bytes = len - done;
        memcpy((uint8_t *)buf + done, fs->cluster_buf + in_cluster, n_bytes);
        done += n_bytes;
    }
    return (long)done;
}

static void fat_release(fs_node_t *node)
{
    kfree(node->priv);
}

/* ---- directories ---- */

// FAT dates count from 1980 in local time; this kernel's clock is UTC
static uint64_t fat_time(uint16_t date, uint16_t time)
{
    if (date == 0)
        return 0;
    date_t d = {
        .year = 1980 + (date >> 9), .month = (date >> 5) & 15, .day = date & 31,
        .hour = time >> 11, .minute = (time >> 5) & 63, .second = (time & 31) * 2,
    };
    if (d.month < 1 || d.month > 12 || d.day < 1 || d.hour > 23 || d.minute > 59 || d.second > 59)
        return 0;
    return (uint64_t)time_from_date(&d);
}

static uint8_t short_checksum(const uint8_t name[11])
{
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++)
        sum = (uint8_t)(((sum & 1) << 7) + (sum >> 1) + name[i]);
    return sum;
}

// "README  TXT" with its lowercase flags -> "readme.txt"
static void short_name(const uint8_t *e, char out[13])
{
    int n = 0;
    for (int i = 0; i < 8 && e[i] != ' '; i++) {
        char c = (char)(i == 0 && e[0] == 0x05 ? 0xE5 : e[i]);  // 0x05 stands for a real 0xE5
        out[n++] = (e[12] & NT_LOWER_BASE) && c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
    }
    if (e[8] != ' ') {
        out[n++] = '.';
        for (int i = 8; i < 11 && e[i] != ' '; i++) {
            char c = (char)e[i];
            out[n++] = (e[12] & NT_LOWER_EXT) && c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
        }
    }
    out[n] = '\0';
    for (int i = 0; i < n; i++) {
        if ((unsigned char)out[i] < 32 || (unsigned char)out[i] > 126)
            out[i] = '?';  // only ASCII names here
    }
}

typedef struct {
    uint16_t chars[LFN_MAX + LFN_CHARS];
    int entries;      // how many long name entries were collected
    int expected;     // how many there are to be
    uint8_t checksum;
} lfn_t;

static void lfn_add(lfn_t *lfn, const uint8_t *e)
{
    int ord = e[0] & 0x3F;
    if (e[0] & LFN_LAST) {
        lfn->expected = ord;
        lfn->entries = 0;
        lfn->checksum = e[13];
        memset(lfn->chars, 0, sizeof(lfn->chars));
    }
    if (ord < 1 || ord > 20 || ord > lfn->expected || e[13] != lfn->checksum) {
        lfn->expected = 0;  // out of order: forget it
        return;
    }
    static const int offsets[LFN_CHARS] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
    for (int i = 0; i < LFN_CHARS; i++)
        lfn->chars[(ord - 1) * LFN_CHARS + i] = le16(e + offsets[i]);
    lfn->entries++;
}

// The long name for the short entry `e`, if one was collected for it
static bool lfn_name(lfn_t *lfn, const uint8_t *e, char *out, size_t size)
{
    if (lfn->expected == 0 || lfn->entries != lfn->expected || short_checksum(e) != lfn->checksum)
        return false;
    size_t n = 0;
    for (int i = 0; i < lfn->expected * LFN_CHARS; i++) {
        uint16_t c = lfn->chars[i];
        if (c == 0x0000 || c == 0xFFFF)
            break;
        if (n + 1 >= size)
            return false;  // too long for this kernel's names: use the short one
        out[n++] = c >= 32 && c < 127 ? (char)c : '?';
    }
    out[n] = '\0';
    return n > 0;
}

// Turn one directory sector's worth of entries into nodes; returns 1 at
// the end-of-directory entry, 0 to go on, or an error
static int parse_entries(fs_node_t *dir, const uint8_t *entries, size_t bytes, lfn_t *lfn)
{
    fat_node_t *parent = dir->priv;
    for (size_t off = 0; off < bytes; off += ENTRY_SIZE) {
        const uint8_t *e = entries + off;
        if (e[0] == 0x00)
            return 1;  // no entries after this
        if (e[0] == 0xE5) {
            lfn->expected = 0;  // deleted
            continue;
        }
        uint8_t attr = e[11];
        if ((attr & 0x3F) == ATTR_LONG_NAME) {
            lfn_add(lfn, e);
            continue;
        }
        if (attr & ATTR_VOLUME_ID || e[0] == '.') {
            lfn->expected = 0;  // the volume label, "." and ".."
            continue;
        }

        char name[FS_NAME_MAX];
        if (!lfn_name(lfn, e, name, sizeof(name)))
            short_name(e, name);
        lfn->expected = 0;

        fat_node_t *n = kcalloc(1, sizeof(*n));
        fs_node_t *child = n ? fs_new_child(dir, name, attr & ATTR_DIRECTORY) : NULL;
        if (child == NULL) {
            kfree(n);
            return -ENOMEM;
        }
        n->fs = parent->fs;
        n->first = (uint32_t)le16(e + 26) | (parent->fs->bits == 32 ? (uint32_t)le16(e + 20) << 16 : 0);
        child->priv = n;
        child->size = child->is_dir ? 0 : le32(e + 28);
        child->mtime = fat_time(le16(e + 24), le16(e + 22));
    }
    return 0;
}

static int fat_load_dir(fs_node_t *dir)
{
    fat_node_t *n = dir->priv;
    fat_fs_t *fs = n->fs;
    lfn_t lfn = { .expected = 0 };
    int r = 0;

    if (n->first == 0 && fs->bits == 16) {
        // FAT16's root directory: a fixed run of sectors
        for (uint32_t s = 0; r == 0 && s < fs->root_sectors; s++) {
            r = fs->dev->read(fs->dev, fs->root_lba + s, 1, fs->cluster_buf);
            if (r == 0)
                r = parse_entries(dir, fs->cluster_buf, SECTOR_SIZE, &lfn);
        }
        return r < 0 ? r : 0;
    }

    uint32_t cluster = n->first;
    uint32_t max_clusters = DIR_ENTRIES_MAX * ENTRY_SIZE / fs->cluster_bytes;
    loop_check_t check = { .saved = cluster, .limit = 1 };
    for (uint32_t count = 0; r == 0 && cluster != END_OF_CHAIN; count++) {
        if (!valid_cluster(fs, cluster) || count >= max_clusters)
            return -EIO;
        r = fs->dev->read(fs->dev, cluster_lba(fs, cluster), fs->sectors_per_cluster, fs->cluster_buf);
        if (r == 0)
            r = parse_entries(dir, fs->cluster_buf, fs->cluster_bytes, &lfn);
        if (r == 0)
            r = next_cluster(fs, cluster, &cluster);
        if (r == 0 && cluster != END_OF_CHAIN && looped(&check, cluster))
            r = -EIO;
    }
    return r < 0 ? r : 0;
}

static const fs_ops_t fat_ops = {
    .name = "fat",
    .load_dir = fat_load_dir,
    .read = fat_read,
    .release = fat_release,
};

/* ---- mounting ---- */

// A sector that starts like a FAT boot sector
static bool looks_like_bpb(const uint8_t *s)
{
    uint8_t spc = s[13];
    return (s[0] == 0xEB || s[0] == 0xE9) && le16(s + 11) == SECTOR_SIZE &&
           spc != 0 && (spc & (spc - 1)) == 0 && s[16] != 0 && le16(s + 14) != 0;
}

fs_node_t *fat_mount(blockdev_t *dev, fat_info_t *info, int *error)
{
    uint8_t s[SECTOR_SIZE];
    *error = -EIO;
    if (dev->read(dev, 0, 1, s) < 0)
        return NULL;
    *error = -EINVAL;
    if (s[510] != 0x55 || s[511] != 0xAA)
        return NULL;

    // The whole disk, or the first FAT partition of an MBR
    uint64_t start = 0;
    if (!looks_like_bpb(s)) {
        for (int i = 0; i < 4 && start == 0; i++) {
            const uint8_t *p = s + 446 + i * 16;
            uint8_t type = p[4];
            if (type == 0x04 || type == 0x06 || type == 0x0E || type == 0x0B || type == 0x0C)
                start = le32(p + 8);
        }
        if (start == 0 || start >= dev->sectors)
            return NULL;
        if (dev->read(dev, start, 1, s) < 0) {
            *error = -EIO;
            return NULL;
        }
        if (!looks_like_bpb(s))
            return NULL;
    }

    uint32_t spc = s[13], reserved = le16(s + 14), fats = s[16], root_entries = le16(s + 17);
    uint32_t total = le16(s + 19) ? le16(s + 19) : le32(s + 32);
    uint32_t fat_size = le16(s + 22) ? le16(s + 22) : le32(s + 36);
    uint32_t root_sectors = (root_entries * ENTRY_SIZE + SECTOR_SIZE - 1) / SECTOR_SIZE;
    uint64_t meta = reserved + (uint64_t)fats * fat_size + root_sectors;
    if (fat_size == 0 || total <= meta || start + total > dev->sectors)
        return NULL;
    uint32_t clusters = (uint32_t)((total - meta) / spc);
    int bits = clusters < 4085 ? 12 : clusters < 65525 ? 16 : 32;
    if (bits == 12)
        return NULL;  // FAT12 (floppies) is not supported
    if (bits == 32 && (root_entries != 0 || le16(s + 22) != 0))
        return NULL;

    fat_fs_t *fs = kcalloc(1, sizeof(*fs));
    fat_node_t *root_node = kcalloc(1, sizeof(*root_node));
    uint8_t *buf = kmalloc(spc * SECTOR_SIZE > SECTOR_SIZE ? spc * SECTOR_SIZE : SECTOR_SIZE);
    fs_node_t *root = NULL;
    if (fs && root_node && buf)
        root = fs_new_root(&fat_ops, root_node, false);
    if (root == NULL) {
        kfree(fs);
        kfree(root_node);
        kfree(buf);
        *error = -ENOMEM;
        return NULL;
    }

    fs->dev = dev;
    fs->bits = bits;
    fs->sectors_per_cluster = spc;
    fs->cluster_bytes = spc * SECTOR_SIZE;
    fs->fat_lba = start + reserved;
    fs->root_lba = start + reserved + (uint64_t)fats * fat_size;
    fs->root_sectors = root_sectors;
    fs->data_lba = start + meta;
    fs->clusters = clusters;
    fs->root_cluster = bits == 32 ? le32(s + 44) : 0;
    fs->cluster_buf = buf;
    root_node->fs = fs;
    root_node->first = fs->root_cluster;

    if (info) {
        info->bits = bits;
        info->bytes = (uint64_t)total * SECTOR_SIZE;
        info->cluster_bytes = fs->cluster_bytes;
        // The label, if the extended boot record has one
        const uint8_t *label = bits == 32 ? s + 71 : s + 43;
        bool has_label = (bits == 32 ? s[66] : s[38]) == 0x29;
        int n = 0;
        for (int i = 0; has_label && i < 11; i++)
            info->label[n++] = label[i] >= 32 && label[i] < 127 ? (char)label[i] : '?';
        while (n > 0 && info->label[n - 1] == ' ')
            n--;
        info->label[n] = '\0';
        if (strcmp(info->label, "NO NAME") == 0)
            info->label[0] = '\0';
    }
    *error = 0;
    return root;
}
