// tar.c
// <kernel/fs.h>
//
// ustar archives: a sequence of 512-byte headers, each followed by the
// member's data padded to 512 bytes, ended by two zero blocks. Numbers are
// octal text. Only what the ramdisk needs is supported: regular files and
// directories, with the ustar prefix field for long paths.

#include <kernel/fs.h>
#include <string.h>

#define BLOCK 512

typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char linkname[100];
    char magic[6];     // "ustar" + NUL (POSIX) or "ustar " (GNU)
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} __attribute__((packed)) tar_header_t;

_Static_assert(sizeof(tar_header_t) == BLOCK, "tar header must be one block");

// Parse an octal field; fields end at a space or NUL. Returns false for
// any other character or an overflow.
static bool octal(const char *field, size_t len, uint64_t *out)
{
    uint64_t value = 0;
    size_t i = 0;

    while (i < len && field[i] == ' ')
        i++;
    for (; i < len && field[i] != '\0' && field[i] != ' '; i++) {
        if (field[i] < '0' || field[i] > '7' || value > (UINT64_MAX >> 3))
            return false;
        value = value * 8 + (uint64_t)(field[i] - '0');
    }
    *out = value;
    return true;
}

static bool is_zero_block(const uint8_t *block)
{
    for (int i = 0; i < BLOCK; i++) {
        if (block[i])
            return false;
    }
    return true;
}

// The checksum is the sum of all header bytes with the checksum field
// itself counted as spaces
static bool checksum_ok(const tar_header_t *h)
{
    uint64_t stored;
    if (!octal(h->checksum, sizeof(h->checksum), &stored))
        return false;

    const uint8_t *bytes = (const uint8_t *)h;
    uint64_t sum = 0;
    for (int i = 0; i < BLOCK; i++) {
        bool in_checksum = i >= (int)offsetof(tar_header_t, checksum) &&
                           i < (int)(offsetof(tar_header_t, checksum) + sizeof(h->checksum));
        sum += in_checksum ? ' ' : bytes[i];
    }
    return sum == stored;
}

// Copy a field that may fill its whole width without a terminating NUL
static size_t copy_field(char *dst, size_t dst_size, const char *src, size_t src_len)
{
    size_t n = 0;
    while (n < src_len && src[n] != '\0' && n + 1 < dst_size) {
        dst[n] = src[n];
        n++;
    }
    dst[n] = '\0';
    return n;
}

int tar_parse(const uint8_t *archive, size_t size,
              bool (*entry)(const tar_entry_t *e, void *ctx), void *ctx)
{
    size_t offset = 0;
    int count = 0;

    while (offset + BLOCK <= size) {
        const uint8_t *block = archive + offset;
        if (is_zero_block(block))
            return count;  // end of archive

        const tar_header_t *h = (const tar_header_t *)block;
        if (memcmp(h->magic, "ustar", 5) != 0 || !checksum_ok(h))
            return -1;

        uint64_t member_size;
        if (!octal(h->size, sizeof(h->size), &member_size))
            return -1;

        size_t data_offset = offset + BLOCK;
        if (member_size > size - data_offset)
            return -1;  // member runs past the end

        tar_entry_t e;
        size_t len = copy_field(e.path, sizeof(e.path), h->prefix, sizeof(h->prefix));
        if (len > 0 && len + 1 < sizeof(e.path))
            e.path[len++] = '/';
        copy_field(e.path + len, sizeof(e.path) - len, h->name, sizeof(h->name));
        e.data = archive + data_offset;
        e.size = (size_t)member_size;
        e.type = h->type == '\0' ? '0' : h->type;  // old archives use NUL for files
        if (!octal(h->mtime, sizeof(h->mtime), &e.mtime))
            e.mtime = 0;  // a broken time is no reason to drop the file

        count++;
        if (!entry(&e, ctx))
            return count;

        // Data is padded to a whole number of blocks
        size_t padded = ((size_t)member_size + BLOCK - 1) / BLOCK * BLOCK;
        if (padded > size - data_offset)
            return -1;
        offset = data_offset + padded;
    }

    return count;  // archive without the end marker: accept what was read
}
