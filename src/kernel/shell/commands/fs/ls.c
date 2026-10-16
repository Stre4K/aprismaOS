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

    fs_lock();  // entries cannot change while they are listed
    fs_node_t *node = fs_lookup(dir);
    if (node == NULL) {
        printk("ls: %s: no such file or directory\n", dir);
    } else if (!node->is_dir) {
        printk("%s  %u bytes\n", dir, (unsigned)node->size);
    } else {
        fs_node_t *entry;
        for (size_t i = 0; (entry = fs_child(node, i)) != NULL; i++) {
            if (entry->is_dir)
                printk("%s/\n", entry->name);
            else
                printk("%-20s %u bytes\n", entry->name, (unsigned)entry->size);
        }
    }
    fs_unlock();
}

SHELL_CMD("fs", "ls", "List a directory", cmd_ls);

#endif // CONFIG_CMD_FS
