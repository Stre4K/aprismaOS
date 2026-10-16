// fs.c
// <kernel/fs.h>
//
// Mounting: the ramdisk is parsed once at boot into the tree (tree.c).
// Its files point into the archive, which stays in memory (the PMM keeps
// Multiboot modules reserved). /tmp is added and made writable.

#include <kernel/fs.h>
#include <kernel/arch/memlayout.h>
#include <kernel/mm/paging.h>
#include <kernel/printk.h>
#include <string.h>

#define MB_FLAG_MODS (1 << 3)

static bool mounted;

static bool add_entry(const tar_entry_t *e, void *ctx)
{
    int *count = ctx;
    if (e->type != '0' && e->type != '5')
        return true;  // links, devices, ... are not supported

    // The archive's own root ("./") is already there
    const char *p = e->path;
    while (*p == '.' || *p == '/')
        p++;
    if (*p == '\0')
        return true;

    if (fs_add_static(e->path, e->type == '5', e->data, e->type == '5' ? 0 : e->size) < 0)
        printk("[FS] Skipping %s\n", e->path);
    else
        (*count)++;
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

    fs_tree_init();
    int count = 0;
    if (tar_parse(archive, size, add_entry, &count) < 0 || count == 0) {
        fs_tree_init();  // nothing usable: back to an empty tree
        return -1;
    }

    // Somewhere to write: /tmp, even if the archive has no such directory
    if (fs_add_static("/tmp", true, NULL, 0) < 0 || fs_make_writable("/tmp") < 0)
        printk("[FS] Cannot make /tmp\n");

    mounted = true;
    return count;
}

bool fs_mounted(void)
{
    return mounted;
}
