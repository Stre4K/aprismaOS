// ls.c
// <kernel/shell/commands.h>

#include <kernel/config.h>

#ifdef CONFIG_CMD_FS

#include <kernel/shell/commands.h>
#include <kernel/fs.h>
#include <kernel/printk.h>

void cmd_ls(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "/";

    if (!fs_mounted()) {
        printk("ls: no filesystem (boot with a ramdisk)\n");
        return;
    }

    const fs_node_t *node = fs_lookup(dir);
    if (node == NULL) {
        printk("ls: %s: no such file or directory\n", dir);
        return;
    }
    if (!node->is_dir) {
        printk("%s  %u bytes\n", dir, (unsigned)node->size);
        return;
    }

    const char *name;
    for (size_t i = 0; (node = fs_list(dir, i, &name)) != NULL; i++) {
        if (node->is_dir)
            printk("%s/\n", name);
        else
            printk("%-20s %u bytes\n", name, (unsigned)node->size);
    }
}

SHELL_CMD("fs", "ls", "List a directory", cmd_ls);

#endif // CONFIG_CMD_FS
