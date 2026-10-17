// test_tar.c
// ustar parsing: src/kernel/fs/tar.c

#include "aprisma.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

#define BLOCK 512

// Write a ustar header (and the data, padded) at `offset`; returns the
// offset after the member
static size_t add_member(uint8_t *ar, size_t offset, const char *prefix, const char *name,
                         char type, const char *data, size_t size)
{
    uint8_t *h = ar + offset;
    memset(h, 0, BLOCK);
    strncpy((char *)h, name, 100);
    memcpy(h + 100, "0000644", 7);
    snprintf((char *)h + 124, 12, "%011o", (unsigned)size);
    h[156] = (uint8_t)type;
    memcpy(h + 257, "ustar", 6);
    memcpy(h + 263, "00", 2);
    if (prefix)
        strncpy((char *)h + 345, prefix, 155);

    // Checksum: header bytes with the checksum field counted as spaces
    memset(h + 148, ' ', 8);
    unsigned sum = 0;
    for (int i = 0; i < BLOCK; i++)
        sum += h[i];
    snprintf((char *)h + 148, 8, "%06o", sum);
    h[155] = ' ';

    memcpy(h + BLOCK, data, size);
    return offset + BLOCK + (size + BLOCK - 1) / BLOCK * BLOCK;
}

typedef struct {
    int count;
    tar_entry_t entries[8];
    int stop_after;  // 0 = never stop
} collected_t;

static bool collect(const tar_entry_t *e, void *ctx)
{
    collected_t *c = ctx;
    if (c->count < 8)
        c->entries[c->count] = *e;
    c->count++;
    return c->stop_after == 0 || c->count < c->stop_after;
}

static uint8_t archive[16 * BLOCK];

TEST(tar_reads_files_and_directories)
{
    memset(archive, 0, sizeof(archive));
    size_t off = add_member(archive, 0, NULL, "./bin/", '5', "", 0);
    off = add_member(archive, off, NULL, "./bin/hello", '0', "hello world", 11);
    off = add_member(archive, off, NULL, "./etc/motd", '0', "welcome\n", 8);

    collected_t c = { 0 };
    CHECK_INT_EQ(aprisma_tar_parse(archive, off + 2 * BLOCK, collect, &c), 3);
    CHECK_STR_EQ(c.entries[0].path, "./bin/");
    CHECK_INT_EQ(c.entries[0].type, '5');
    CHECK_STR_EQ(c.entries[1].path, "./bin/hello");
    CHECK_INT_EQ(c.entries[1].size, 11);
    CHECK_MEM_EQ(c.entries[1].data, "hello world", 11);
    CHECK(c.entries[1].data == archive + 2 * BLOCK);  // points into the archive
    CHECK_INT_EQ(c.entries[2].size, 8);
    CHECK_MEM_EQ(c.entries[2].data, "welcome\n", 8);
}

TEST(tar_joins_prefix_and_name)
{
    memset(archive, 0, sizeof(archive));
    size_t off = add_member(archive, 0, "a/long/directory", "file.txt", '0', "x", 1);

    collected_t c = { 0 };
    CHECK_INT_EQ(aprisma_tar_parse(archive, off + 2 * BLOCK, collect, &c), 1);
    CHECK_STR_EQ(c.entries[0].path, "a/long/directory/file.txt");
}

TEST(tar_treats_nul_type_as_regular_file)
{
    memset(archive, 0, sizeof(archive));
    size_t off = add_member(archive, 0, NULL, "old", '\0', "data", 4);

    collected_t c = { 0 };
    CHECK_INT_EQ(aprisma_tar_parse(archive, off + 2 * BLOCK, collect, &c), 1);
    CHECK_INT_EQ(c.entries[0].type, '0');
}

TEST(tar_data_padding_is_skipped)
{
    memset(archive, 0, sizeof(archive));
    char big[700];
    memset(big, 'B', sizeof(big));
    size_t off = add_member(archive, 0, NULL, "big", '0', big, sizeof(big));  // spans 2 blocks
    off = add_member(archive, off, NULL, "after", '0', "A", 1);

    collected_t c = { 0 };
    CHECK_INT_EQ(aprisma_tar_parse(archive, off + 2 * BLOCK, collect, &c), 2);
    CHECK_STR_EQ(c.entries[1].path, "after");
    CHECK_MEM_EQ(c.entries[1].data, "A", 1);
}

TEST(tar_rejects_a_bad_checksum)
{
    memset(archive, 0, sizeof(archive));
    size_t off = add_member(archive, 0, NULL, "file", '0', "data", 4);
    archive[0] = 'F';  // change the name after the checksum was computed

    collected_t c = { 0 };
    CHECK_INT_EQ(aprisma_tar_parse(archive, off, collect, &c), -1);
    CHECK_INT_EQ(c.count, 0);
}

TEST(tar_rejects_a_missing_magic)
{
    memset(archive, 0, sizeof(archive));
    memset(archive, 'x', BLOCK);  // not a tar header at all

    collected_t c = { 0 };
    CHECK_INT_EQ(aprisma_tar_parse(archive, BLOCK, collect, &c), -1);
}

TEST(tar_rejects_a_member_past_the_end)
{
    memset(archive, 0, sizeof(archive));
    add_member(archive, 0, NULL, "file", '0', "data", 4);
    snprintf((char *)archive + 124, 12, "%011o", 100000u);  // claims 100000 bytes
    // fix the checksum so only the size is wrong
    memset(archive + 148, ' ', 8);
    unsigned sum = 0;
    for (int i = 0; i < BLOCK; i++)
        sum += archive[i];
    snprintf((char *)archive + 148, 8, "%06o", sum);
    archive[155] = ' ';

    collected_t c = { 0 };
    CHECK_INT_EQ(aprisma_tar_parse(archive, 4 * BLOCK, collect, &c), -1);
    CHECK_INT_EQ(c.count, 0);
}

TEST(tar_stops_when_the_callback_says_so)
{
    memset(archive, 0, sizeof(archive));
    size_t off = add_member(archive, 0, NULL, "one", '0', "1", 1);
    off = add_member(archive, off, NULL, "two", '0', "2", 1);
    off = add_member(archive, off, NULL, "three", '0', "3", 1);

    collected_t c = { .stop_after = 2 };
    CHECK_INT_EQ(aprisma_tar_parse(archive, off + 2 * BLOCK, collect, &c), 2);
}

TEST(tar_empty_archive)
{
    memset(archive, 0, sizeof(archive));
    collected_t c = { 0 };
    CHECK_INT_EQ(aprisma_tar_parse(archive, 2 * BLOCK, collect, &c), 0);
    CHECK_INT_EQ(aprisma_tar_parse(archive, 0, collect, &c), 0);  // no bytes at all
}

TEST(tar_reads_the_modification_time)
{
    memset(archive, 0, sizeof(archive));
    size_t off = add_member(archive, 0, NULL, "./etc/motd", '0', "hi\n", 3);
    // Set the time (octal seconds) and fix the checksum, as add_member does
    uint8_t *h = archive;
    memcpy(h + 136, "14524770462", 11);  // 1700000050
    memset(h + 148, ' ', 8);
    unsigned sum = 0;
    for (int i = 0; i < BLOCK; i++)
        sum += h[i];
    snprintf((char *)h + 148, 8, "%06o", sum);
    h[155] = ' ';

    collected_t c = { 0 };
    CHECK_INT_EQ(aprisma_tar_parse(archive, off + 2 * BLOCK, collect, &c), 1);
    CHECK_INT_EQ(c.entries[0].mtime, 1700000050);
}
