// test_fs.c
// The filesystem tree: src/kernel/fs/tree.c, with kmalloc and friends
// backed by the host's malloc and counted, so every test can check that
// nothing leaked. Each test starts with a fresh tree.

#include "aprisma.h"
#include "test.h"

#include <kernel/abi/syscall.h>
#include <stdlib.h>
#include <string.h>

void aprisma_fs_tree_init(void);
int aprisma_fs_add_static(const char *path, bool is_dir, const uint8_t *data, size_t size, uint64_t mtime);
void aprisma_fs_set_clock(uint64_t (*now)(void));
int aprisma_fs_make_writable(const char *path);
int aprisma_fs_resolve(const char *path, fs_node_t **out);
fs_node_t *aprisma_fs_lookup(const char *path);
int aprisma_fs_open(const char *path, int flags, fs_node_t **out);
void aprisma_fs_close(fs_node_t *node);
int aprisma_fs_create(const char *path, bool is_dir, fs_node_t **out);
int aprisma_fs_remove(const char *path, bool is_dir);
long aprisma_fs_read(fs_node_t *node, size_t offset, void *buf, size_t len);
long aprisma_fs_write(fs_node_t *node, size_t offset, const void *buf, size_t len);
int aprisma_fs_truncate(fs_node_t *node, size_t size);
fs_node_t *aprisma_fs_child(fs_node_t *dir, size_t index);
size_t aprisma_fs_space_used(void);

/* ---- the kernel heap, on the host ---- */

static long live_blocks;   // allocated and not yet freed
static int fail_after = -1; // fail this many allocations from now (-1: never)

static int should_fail(void)
{
    if (fail_after < 0)
        return 0;
    return fail_after-- == 0;
}

void *aprisma_kmalloc(size_t size)
{
    if (should_fail())
        return NULL;
    live_blocks++;
    return malloc(size ? size : 1);
}

void *aprisma_kcalloc(size_t count, size_t size)
{
    if (should_fail())
        return NULL;
    live_blocks++;
    return calloc(count ? count : 1, size ? size : 1);
}

void *aprisma_krealloc(void *ptr, size_t size)
{
    if (should_fail())
        return NULL;
    if (ptr == NULL)
        live_blocks++;
    return realloc(ptr, size);
}

void aprisma_kfree(void *ptr)
{
    if (ptr)
        live_blocks--;
    free(ptr);
}

static const uint8_t motd[] = "Welcome!\n";
static long baseline;  // blocks in use with a fresh tree (other tests may keep some, e.g. a FAT mount's)

// A tree like the one the kernel builds: /etc/motd and /bin from the
// ramdisk (read-only), /tmp writable
static void fresh_tree(void)
{
    fail_after = -1;
    aprisma_fs_tree_init();
    aprisma_fs_add_static("./etc/motd", false, motd, sizeof(motd) - 1, 1000);
    aprisma_fs_add_static("bin/", true, NULL, 0, 1000);
    aprisma_fs_add_static("/tmp", true, NULL, 0, 1000);
    aprisma_fs_make_writable("/tmp");
    baseline = live_blocks;
}

static long blocks_of_empty_tree(void)
{
    return baseline;
}

/* ---- tests ---- */

TEST(fs_paths_are_walked_like_unix_paths)
{
    fresh_tree();
    fs_node_t *motd_node = aprisma_fs_lookup("/etc/motd");
    CHECK(motd_node != NULL && !motd_node->is_dir);
    CHECK_STR_EQ(motd_node->name, "motd");
    CHECK(aprisma_fs_lookup("etc/motd") == motd_node);           // no current directory: from the root
    CHECK(aprisma_fs_lookup("//etc///motd") == motd_node);
    CHECK(aprisma_fs_lookup("/etc/./motd") == motd_node);
    CHECK(aprisma_fs_lookup("/bin/../etc/motd") == motd_node);
    CHECK(aprisma_fs_lookup("/../../etc/motd") == motd_node);     // the root is its own parent
    CHECK(aprisma_fs_lookup("/") != NULL && aprisma_fs_lookup("/")->is_dir);
    CHECK(aprisma_fs_lookup("") == aprisma_fs_lookup("/"));

    fs_node_t *node;
    CHECK_INT_EQ(aprisma_fs_resolve("/etc/nope", &node), -ENOENT);
    CHECK_INT_EQ(aprisma_fs_resolve("/etc/motd/x", &node), -ENOTDIR);
    char long_name[FS_NAME_MAX + 2];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    CHECK_INT_EQ(aprisma_fs_resolve(long_name, &node), -ENAMETOOLONG);
    char long_path[FS_PATH_MAX + 1];
    memset(long_path, '/', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';
    CHECK_INT_EQ(aprisma_fs_resolve(long_path, &node), -ENAMETOOLONG);
}

TEST(fs_ramdisk_entries_are_read_only)
{
    fresh_tree();
    fs_node_t *motd_node = aprisma_fs_lookup("/etc/motd");
    char buf[32];
    CHECK_INT_EQ(aprisma_fs_read(motd_node, 0, buf, sizeof(buf)), 9);
    CHECK_MEM_EQ(buf, "Welcome!\n", 9);
    CHECK_INT_EQ(aprisma_fs_read(motd_node, 3, buf, 2), 2);
    CHECK_MEM_EQ(buf, "co", 2);
    CHECK_INT_EQ(aprisma_fs_read(motd_node, 9, buf, 1), 0);    // end of file
    CHECK_INT_EQ(aprisma_fs_read(motd_node, 100, buf, 1), 0);

    CHECK_INT_EQ(aprisma_fs_write(motd_node, 0, "x", 1), -EROFS);
    CHECK_INT_EQ(aprisma_fs_truncate(motd_node, 0), -EROFS);
    fs_node_t *node;
    CHECK_INT_EQ(aprisma_fs_open("/etc/motd", O_WRONLY, &node), -EROFS);
    CHECK_INT_EQ(aprisma_fs_open("/etc/new", O_WRONLY | O_CREAT, &node), -EROFS);
    CHECK_INT_EQ(aprisma_fs_create("/bin/x", false, &node), -EROFS);
    CHECK_INT_EQ(aprisma_fs_remove("/etc/motd", false), -EROFS);
    CHECK_INT_EQ(aprisma_fs_remove("/tmp", true), -EROFS);      // its parent is read-only
    CHECK_INT_EQ(aprisma_fs_remove("/", true), -EINVAL);
    CHECK(aprisma_fs_lookup("/etc/motd")->data == motd);       // never copied
}

TEST(fs_files_grow_as_they_are_written)
{
    fresh_tree();
    fs_node_t *f;
    CHECK_INT_EQ(aprisma_fs_open("/tmp/a", O_RDWR | O_CREAT, &f), 0);
    CHECK(f->writable && f->size == 0 && f->open_count == 1);

    CHECK_INT_EQ(aprisma_fs_write(f, 0, "hello", 5), 5);
    CHECK_INT_EQ(aprisma_fs_write(f, 5, " world", 6), 6);
    char buf[64];
    CHECK_INT_EQ(aprisma_fs_read(f, 0, buf, sizeof(buf)), 11);
    CHECK_MEM_EQ(buf, "hello world", 11);

    CHECK_INT_EQ(aprisma_fs_write(f, 0, "J", 1), 1);            // overwrite in place
    CHECK_INT_EQ(aprisma_fs_write(f, FS_APPEND, "!", 1), 1);    // at the end
    CHECK_INT_EQ(aprisma_fs_read(f, 0, buf, sizeof(buf)), 12);
    CHECK_MEM_EQ(buf, "Jello world!", 12);

    CHECK_INT_EQ(aprisma_fs_write(f, 20, "z", 1), 1);           // past the end: a gap of zeros
    CHECK_INT_EQ(f->size, 21);
    CHECK_INT_EQ(aprisma_fs_read(f, 12, buf, 9), 9);
    CHECK_MEM_EQ(buf, "\0\0\0\0\0\0\0\0z", 9);

    // Many small writes: the buffer grows, contents stay
    for (int i = 0; i < 5000; i++) {
        char c = (char)('a' + i % 26);
        CHECK_INT_EQ(aprisma_fs_write(f, FS_APPEND, &c, 1), 1);
    }
    CHECK_INT_EQ(f->size, 5021);
    CHECK(f->capacity >= f->size && aprisma_fs_space_used() == f->capacity);
    CHECK_INT_EQ(aprisma_fs_read(f, 21 + 4999, buf, 1), 1);
    CHECK(buf[0] == 'a' + 4999 % 26);

    aprisma_fs_close(f);
    CHECK(aprisma_fs_lookup("/tmp/a") == f);  // closing does not remove it
}

TEST(fs_truncate_shrinks_grows_and_frees)
{
    fresh_tree();
    fs_node_t *f;
    CHECK_INT_EQ(aprisma_fs_open("/tmp/t", O_WRONLY | O_CREAT, &f), 0);
    aprisma_fs_write(f, 0, "abcdef", 6);
    CHECK_INT_EQ(aprisma_fs_truncate(f, 3), 0);
    CHECK_INT_EQ(f->size, 3);
    CHECK_INT_EQ(aprisma_fs_truncate(f, 8), 0);   // longer again: zeros, not the old bytes
    char buf[8];
    CHECK_INT_EQ(aprisma_fs_read(f, 0, buf, 8), 8);
    CHECK_MEM_EQ(buf, "abc\0\0\0\0\0", 8);
    CHECK_INT_EQ(aprisma_fs_truncate(f, 0), 0);
    CHECK(f->data == NULL && f->capacity == 0 && aprisma_fs_space_used() == 0);

    // O_TRUNC empties on open
    aprisma_fs_write(f, 0, "old", 3);
    aprisma_fs_close(f);
    CHECK_INT_EQ(aprisma_fs_open("/tmp/t", O_WRONLY | O_TRUNC, &f), 0);
    CHECK_INT_EQ(f->size, 0);
    aprisma_fs_close(f);
    CHECK_INT_EQ(aprisma_fs_open("/tmp/t", O_RDONLY | O_TRUNC, &f), 0);  // only when writing
    aprisma_fs_close(f);
}

TEST(fs_open_checks_flags_and_kinds)
{
    fresh_tree();
    fs_node_t *node;
    CHECK_INT_EQ(aprisma_fs_open("/tmp/x", O_RDONLY, &node), -ENOENT);
    CHECK_INT_EQ(aprisma_fs_open("/tmp/nodir/x", O_WRONLY | O_CREAT, &node), -ENOENT);
    CHECK_INT_EQ(aprisma_fs_open("/tmp", O_RDONLY, &node), -EISDIR);
    CHECK_INT_EQ(aprisma_fs_open("/tmp", O_WRONLY | O_CREAT, &node), -EISDIR);
    CHECK_INT_EQ(aprisma_fs_open("/tmp/x", O_ACCMODE, &node), -EINVAL);
    CHECK_INT_EQ(aprisma_fs_open("/tmp/x", O_WRONLY | 0x10000, &node), -EINVAL);
    CHECK_INT_EQ(aprisma_fs_open("/etc/motd/x", O_WRONLY | O_CREAT, &node), -ENOTDIR);
    CHECK(aprisma_fs_lookup("/tmp/x") == NULL);  // nothing made by the failures
}

TEST(fs_create_and_remove_follow_the_rules)
{
    fresh_tree();
    fs_node_t *dir, *file;
    CHECK_INT_EQ(aprisma_fs_create("/tmp/d", true, &dir), 0);
    CHECK_INT_EQ(aprisma_fs_create("/tmp/d/f", false, &file), 0);
    CHECK_INT_EQ(aprisma_fs_create("/tmp/d", true, NULL), -EEXIST);
    CHECK_INT_EQ(aprisma_fs_create("/tmp/d/", true, NULL), -EEXIST);
    CHECK_INT_EQ(aprisma_fs_create("/tmp/d/f/g", false, NULL), -ENOTDIR);
    CHECK_INT_EQ(aprisma_fs_create("/tmp/none/g", false, NULL), -ENOENT);
    CHECK_INT_EQ(aprisma_fs_create("/tmp/..", true, NULL), -EEXIST);
    CHECK_INT_EQ(aprisma_fs_create("/", true, NULL), -EEXIST);
    CHECK(file->writable && dir->writable && file->parent == dir);

    CHECK_INT_EQ(aprisma_fs_remove("/tmp/d", true), -ENOTEMPTY);
    CHECK_INT_EQ(aprisma_fs_remove("/tmp/d", false), -EISDIR);
    CHECK_INT_EQ(aprisma_fs_remove("/tmp/d/f", true), -ENOTDIR);
    CHECK_INT_EQ(aprisma_fs_remove("/tmp/d/none", false), -ENOENT);
    CHECK_INT_EQ(aprisma_fs_remove("/tmp/d/f", false), 0);
    CHECK_INT_EQ(aprisma_fs_remove("/tmp/d/", true), 0);
    CHECK(aprisma_fs_lookup("/tmp/d") == NULL);
    CHECK_INT_EQ(live_blocks, blocks_of_empty_tree());
}

TEST(fs_directories_list_entries_in_creation_order)
{
    fresh_tree();
    aprisma_fs_create("/tmp/b", false, NULL);
    aprisma_fs_create("/tmp/a", false, NULL);
    aprisma_fs_create("/tmp/c", true, NULL);
    fs_node_t *tmp = aprisma_fs_lookup("/tmp");
    CHECK_STR_EQ(aprisma_fs_child(tmp, 0)->name, "b");
    CHECK_STR_EQ(aprisma_fs_child(tmp, 1)->name, "a");
    CHECK_STR_EQ(aprisma_fs_child(tmp, 2)->name, "c");
    CHECK(aprisma_fs_child(tmp, 3) == NULL);
    CHECK(aprisma_fs_child(aprisma_fs_lookup("/etc/motd"), 0) == NULL);  // a file has no entries

    aprisma_fs_remove("/tmp/a", false);
    CHECK_STR_EQ(aprisma_fs_child(tmp, 1)->name, "c");
    fs_node_t *root = aprisma_fs_lookup("/");
    CHECK_STR_EQ(aprisma_fs_child(root, 0)->name, "etc");  // the ramdisk's order
    CHECK_STR_EQ(aprisma_fs_child(root, 1)->name, "bin");
    CHECK_STR_EQ(aprisma_fs_child(root, 2)->name, "tmp");
}

TEST(fs_a_file_removed_while_open_lives_until_closed)
{
    fresh_tree();
    fs_node_t *f;
    aprisma_fs_open("/tmp/gone", O_RDWR | O_CREAT, &f);
    aprisma_fs_write(f, 0, "still here", 10);
    fs_node_t *again;
    CHECK_INT_EQ(aprisma_fs_open("/tmp/gone", O_RDONLY, &again), 0);
    CHECK_INT_EQ(f->open_count, 2);

    CHECK_INT_EQ(aprisma_fs_remove("/tmp/gone", false), 0);
    CHECK(aprisma_fs_lookup("/tmp/gone") == NULL);
    CHECK(f->removed);
    char buf[16];
    CHECK_INT_EQ(aprisma_fs_read(f, 0, buf, sizeof(buf)), 10);  // still readable and writable
    CHECK_INT_EQ(aprisma_fs_write(f, FS_APPEND, "!", 1), 1);

    CHECK_INT_EQ(aprisma_fs_create("/tmp/gone", false, NULL), 0);  // the name is free again
    aprisma_fs_close(again);
    CHECK_INT_EQ(live_blocks, blocks_of_empty_tree() + 3);  // the new file, the removed one and its data
    aprisma_fs_close(f);                                   // the last close frees it
    CHECK_INT_EQ(live_blocks, blocks_of_empty_tree() + 1);
}

TEST(fs_limits_give_efbig_and_enospc)
{
    fresh_tree();
    static char chunk[1 << 20];
    memset(chunk, 'x', sizeof(chunk));

    fs_node_t *big;
    aprisma_fs_open("/tmp/big", O_WRONLY | O_CREAT, &big);
    for (size_t off = 0; off < FS_FILE_MAX; off += sizeof(chunk))
        CHECK_INT_EQ(aprisma_fs_write(big, off, chunk, sizeof(chunk)), sizeof(chunk));
    CHECK_INT_EQ(big->size, FS_FILE_MAX);
    CHECK_INT_EQ(aprisma_fs_write(big, FS_APPEND, "x", 1), -EFBIG);
    CHECK_INT_EQ(aprisma_fs_write(big, FS_FILE_MAX - 2, "abcd", 4), 2);  // as much as fits
    CHECK_INT_EQ(aprisma_fs_truncate(big, FS_FILE_MAX + 1), -EFBIG);

    // Fill the rest of the space with more files
    char name[32];
    int files = 1;
    long r = 0;
    for (; files < 100 && r >= 0; files++) {
        fs_node_t *f;
        strcpy(name, "/tmp/f");
        name[6] = (char)('0' + files / 10);
        name[7] = (char)('0' + files % 10);
        name[8] = '\0';
        aprisma_fs_open(name, O_WRONLY | O_CREAT, &f);
        for (size_t off = 0; off < FS_FILE_MAX && (r = aprisma_fs_write(f, off, chunk, sizeof(chunk))) >= 0;)
            off += (size_t)r;
        aprisma_fs_close(f);
    }
    CHECK_INT_EQ(r, -ENOSPC);
    CHECK(aprisma_fs_space_used() <= FS_SPACE_MAX);
    CHECK(aprisma_fs_space_used() > FS_SPACE_MAX - (1 << 20));  // it really filled up

    // Removing a file makes room again (once its last open file is closed)
    CHECK_INT_EQ(aprisma_fs_remove("/tmp/big", false), 0);
    aprisma_fs_close(big);
    CHECK(aprisma_fs_space_used() <= FS_SPACE_MAX - FS_FILE_MAX);
}

TEST(fs_out_of_memory_leaves_the_tree_intact)
{
    fresh_tree();
    fs_node_t *f;
    fail_after = 0;  // the next allocation fails
    CHECK_INT_EQ(aprisma_fs_create("/tmp/x", false, &f), -ENOSPC);
    CHECK(aprisma_fs_lookup("/tmp/x") == NULL);

    CHECK_INT_EQ(aprisma_fs_open("/tmp/y", O_WRONLY | O_CREAT, &f), 0);
    aprisma_fs_write(f, 0, "keep", 4);
    fail_after = 0;
    char more[1000] = { 0 };
    CHECK_INT_EQ(aprisma_fs_write(f, FS_APPEND, more, sizeof(more)), -ENOSPC);  // the buffer could not grow
    char buf[8];
    CHECK_INT_EQ(aprisma_fs_read(f, 0, buf, sizeof(buf)), 4);
    CHECK_MEM_EQ(buf, "keep", 4);
    aprisma_fs_close(f);

    fail_after = 0;
    CHECK_INT_EQ(aprisma_fs_add_static("/new/dir/file", false, motd, 3, 0), -ENOSPC);
    fail_after = -1;
}

TEST(fs_reset_frees_everything)
{
    fresh_tree();
    fs_node_t *f;
    aprisma_fs_create("/tmp/d", true, NULL);
    aprisma_fs_open("/tmp/d/f", O_WRONLY | O_CREAT, &f);
    aprisma_fs_write(f, 0, "data", 4);
    aprisma_fs_close(f);
    fresh_tree();
    CHECK_INT_EQ(live_blocks, blocks_of_empty_tree());
    CHECK_INT_EQ(aprisma_fs_space_used(), 0);
}

int aprisma_fs_rename(const char *from, const char *to);
int aprisma_fs_path_join(const char *cwd, const char *path, char out[FS_PATH_MAX]);

TEST(fs_path_join_makes_paths_absolute)
{
    char out[FS_PATH_MAX];
    CHECK_INT_EQ(aprisma_fs_path_join("/tmp", "a", out), 0);
    CHECK_STR_EQ(out, "/tmp/a");
    CHECK_INT_EQ(aprisma_fs_path_join("/tmp", "/etc/motd", out), 0);  // absolute: cwd ignored
    CHECK_STR_EQ(out, "/etc/motd");
    CHECK_INT_EQ(aprisma_fs_path_join("/tmp/d", "../x/./y//", out), 0);
    CHECK_STR_EQ(out, "/tmp/x/y");
    CHECK_INT_EQ(aprisma_fs_path_join("/", "..", out), 0);           // the root's parent is the root
    CHECK_STR_EQ(out, "/");
    CHECK_INT_EQ(aprisma_fs_path_join("/a/b", "../../../c", out), 0);
    CHECK_STR_EQ(out, "/c");
    CHECK_INT_EQ(aprisma_fs_path_join("/tmp", "", out), 0);          // "" is the directory itself
    CHECK_STR_EQ(out, "/tmp");
    CHECK_INT_EQ(aprisma_fs_path_join("/tmp", ".", out), 0);
    CHECK_STR_EQ(out, "/tmp");

    char name[FS_NAME_MAX + 1];
    memset(name, 'n', FS_NAME_MAX);
    name[FS_NAME_MAX] = '\0';
    CHECK_INT_EQ(aprisma_fs_path_join("/", name, out), -ENAMETOOLONG);
    char deep[FS_PATH_MAX];
    strcpy(deep, "/");
    for (int i = 0; i < 60; i++)
        strcat(deep, "abc/");  // fits as text, but not under a long cwd
    CHECK_INT_EQ(aprisma_fs_path_join("/tmp/some/longer/directory/name/here", deep + 1, out), -ENAMETOOLONG);
}

TEST(fs_rename_moves_and_replaces)
{
    fresh_tree();
    fs_node_t *f, *g;
    aprisma_fs_create("/tmp/d", true, NULL);
    aprisma_fs_open("/tmp/a", O_WRONLY | O_CREAT, &f);
    aprisma_fs_write(f, 0, "aaa", 3);
    CHECK_INT_EQ(aprisma_fs_rename("/tmp/a", "/tmp/d/b"), 0);  // move and rename
    CHECK(aprisma_fs_lookup("/tmp/a") == NULL);
    CHECK(aprisma_fs_lookup("/tmp/d/b") == f && f->parent == aprisma_fs_lookup("/tmp/d"));
    CHECK_STR_EQ(f->name, "b");
    char buf[4];
    CHECK_INT_EQ(aprisma_fs_read(f, 0, buf, 3), 3);  // still the same open file
    CHECK_INT_EQ(aprisma_fs_rename("/tmp/d/b", "/tmp/d/b"), 0);

    // A file replaces a file; the replaced one is freed (or kept while open)
    aprisma_fs_open("/tmp/c", O_WRONLY | O_CREAT, &g);
    CHECK_INT_EQ(aprisma_fs_rename("/tmp/d/b", "/tmp/c"), 0);
    CHECK(aprisma_fs_lookup("/tmp/c") == f && g->removed);
    aprisma_fs_close(g);
    aprisma_fs_close(f);

    // What may not happen
    aprisma_fs_create("/tmp/e", true, NULL);
    CHECK_INT_EQ(aprisma_fs_rename("/tmp/c", "/tmp/d"), -EEXIST);     // a file over a directory
    CHECK_INT_EQ(aprisma_fs_rename("/tmp/e", "/tmp/d"), -EEXIST);     // a directory over a directory
    CHECK_INT_EQ(aprisma_fs_rename("/tmp/d", "/tmp/d/x"), -EINVAL);   // into itself
    CHECK_INT_EQ(aprisma_fs_rename("/tmp", "/tmp/d/x"), -EROFS);      // /tmp's parent is read-only
    CHECK_INT_EQ(aprisma_fs_rename("/etc/motd", "/tmp/m"), -EROFS);
    CHECK_INT_EQ(aprisma_fs_rename("/tmp/c", "/etc/c"), -EROFS);
    CHECK_INT_EQ(aprisma_fs_rename("/tmp/none", "/tmp/x"), -ENOENT);
    CHECK_INT_EQ(aprisma_fs_rename("/tmp/c", "/tmp/none/x"), -ENOENT);
    CHECK_INT_EQ(aprisma_fs_rename("/", "/tmp/x"), -EINVAL);

    // Directories move with what is inside them
    CHECK_INT_EQ(aprisma_fs_rename("/tmp/c", "/tmp/d/c"), 0);
    CHECK_INT_EQ(aprisma_fs_rename("/tmp/d", "/tmp/e/moved"), 0);
    CHECK(aprisma_fs_lookup("/tmp/e/moved/c") == f);
    CHECK_STR_EQ(aprisma_fs_child(aprisma_fs_lookup("/tmp"), 0)->name, "e");  // d is gone from /tmp
}

static uint64_t fake_now;

static uint64_t fake_clock(void)
{
    return fake_now;
}

TEST(fs_times_follow_changes)
{
    fresh_tree();
    aprisma_fs_set_clock(fake_clock);
    CHECK_INT_EQ(aprisma_fs_lookup("/etc/motd")->mtime, 1000);  // from the ramdisk
    CHECK_INT_EQ(aprisma_fs_lookup("/etc")->mtime, 1000);       // made along with it

    fs_node_t *f, *tmp = aprisma_fs_lookup("/tmp");
    fake_now = 2000;
    aprisma_fs_open("/tmp/f", O_WRONLY | O_CREAT, &f);
    CHECK_INT_EQ(f->mtime, 2000);
    CHECK_INT_EQ(tmp->mtime, 2000);       // a directory changes with its entries
    fake_now = 3000;
    aprisma_fs_write(f, 0, "x", 1);
    CHECK_INT_EQ(f->mtime, 3000);
    CHECK_INT_EQ(tmp->mtime, 2000);       // writing a file does not change the directory
    fake_now = 4000;
    aprisma_fs_truncate(f, 0);
    CHECK_INT_EQ(f->mtime, 4000);
    char buf[1];
    fake_now = 5000;
    aprisma_fs_read(f, 0, buf, 1);
    CHECK_INT_EQ(f->mtime, 4000);         // reading is no change
    aprisma_fs_close(f);

    aprisma_fs_create("/tmp/d", true, NULL);
    fake_now = 6000;
    aprisma_fs_rename("/tmp/f", "/tmp/d/f");
    CHECK_INT_EQ(tmp->mtime, 6000);
    CHECK_INT_EQ(aprisma_fs_lookup("/tmp/d")->mtime, 6000);
    CHECK_INT_EQ(aprisma_fs_lookup("/tmp/d/f")->mtime, 4000);  // moving is no change to the file
    fake_now = 7000;
    aprisma_fs_remove("/tmp/d/f", false);
    CHECK_INT_EQ(aprisma_fs_lookup("/tmp/d")->mtime, 7000);
    aprisma_fs_set_clock(NULL);
}
