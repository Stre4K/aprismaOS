// fs.c
// <kernel/fs.h>
//
// The ramdisk is parsed once at boot into a table of nodes pointing into
// the archive, which stays in memory (the PMM keeps Multiboot modules
// reserved). Lookups are linear: the ramdisk holds a few dozen files.

#include <kernel/fs.h>
#include <kernel/arch/memlayout.h>
#include <kernel/mm/heap.h>
#include <kernel/mm/paging.h>
#include <kernel/printk.h>
#include <string.h>

#define MB_FLAG_MODS (1 << 3)

static fs_node_t *nodes;
static size_t node_count;
static bool mounted;

static const fs_node_t root = { .path = "/", .is_dir = true };

// "./bin/hello", "bin/hello/" and "/bin//hello" all become "/bin/hello"
static void normalize(const char *in, char *out, size_t size)
{
    size_t n = 0;
    out[n++] = '/';

    while (*in && n + 1 < size) {
        // skip empty and "." components
        while (*in == '/' || (in[0] == '.' && (in[1] == '/' || in[1] == '\0'))) {
            in += (*in == '/') ? 1 : 1 + (in[1] == '/');
        }
        if (!*in)
            break;
        if (n > 1)
            out[n++] = '/';
        while (*in && *in != '/' && n + 1 < size)
            out[n++] = *in++;
    }
    out[n] = '\0';
}

static bool count_entry(const tar_entry_t *e, void *ctx)
{
    (void)e;
    (*(size_t *)ctx)++;
    return true;
}

static bool add_entry(const tar_entry_t *e, void *ctx)
{
    size_t capacity = *(size_t *)ctx;
    if (e->type != '0' && e->type != '5')
        return true;  // links, devices, ... are not supported
    if (node_count == capacity)
        return false;

    fs_node_t *node = &nodes[node_count];
    normalize(e->path, node->path, sizeof(node->path));
    if (strcmp(node->path, "/") == 0)
        return true;  // the root directory itself

    node->data = e->data;
    node->size = e->type == '5' ? 0 : e->size;
    node->is_dir = e->type == '5';
    node_count++;
    return true;
}

int fs_init(multiboot_info_t *mbi)
{
    if (!(mbi->flags & MB_FLAG_MODS) || mbi->mods_count == 0)
        return -1;

    multiboot_mod_t *mod = (multiboot_mod_t *)P2V(mbi->mods_addr);
    size_t size = mod->mod_end - mod->mod_start;
    const uint8_t *archive = paging_map_physical(mod->mod_start, size);
    if (archive == NULL)
        return -1;

    size_t total = 0;
    if (tar_parse(archive, size, count_entry, &total) < 0 || total == 0)
        return -1;

    nodes = kcalloc(total, sizeof(*nodes));
    if (nodes == NULL)
        return -1;
    tar_parse(archive, size, add_entry, &total);

    mounted = true;
    return (int)node_count;
}

bool fs_mounted(void)
{
    return mounted;
}

const fs_node_t *fs_lookup(const char *path)
{
    char normalized[FS_PATH_MAX];
    normalize(path, normalized, sizeof(normalized));

    if (strcmp(normalized, "/") == 0)
        return mounted ? &root : NULL;

    for (size_t i = 0; i < node_count; i++) {
        if (strcmp(nodes[i].path, normalized) == 0)
            return &nodes[i];
    }
    return NULL;
}

// Length of the directory part of a path: "/bin/hello" -> 4, "/motd" -> 0
static size_t parent_length(const char *path)
{
    size_t last = 0;
    for (size_t i = 0; path[i]; i++) {
        if (path[i] == '/')
            last = i;
    }
    return last;
}

const fs_node_t *fs_list(const char *dir, size_t index, const char **name)
{
    char normalized[FS_PATH_MAX];
    normalize(dir, normalized, sizeof(normalized));
    size_t dir_len = strcmp(normalized, "/") == 0 ? 0 : strlen(normalized);

    for (size_t i = 0; i < node_count; i++) {
        const char *path = nodes[i].path;
        if (parent_length(path) == dir_len && strncmp(path, normalized, dir_len) == 0) {
            if (index-- == 0) {
                *name = path + dir_len + 1;
                return &nodes[i];
            }
        }
    }
    return NULL;
}
