// cat.c
// <kernel/shell/commands.h>

#include <kernel/config.h>

#ifdef CONFIG_CMD_FS

#include <kernel/shell/commands.h>
#include <kernel/fs.h>
#include <kernel/printk.h>

void cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        printk("Usage: cat <file>\n");
        return;
    }

    fs_lock();  // the file cannot change while it is printed
    const fs_node_t *node = fs_lookup(argv[1]);
    if (node == NULL) {
        printk("cat: %s: no such file\n", argv[1]);
    } else if (node->is_dir) {
        printk("cat: %s: is a directory\n", argv[1]);
    } else {
        printk("%.*s", (int)node->size, (const char *)node->data);
        if (node->size > 0 && node->data[node->size - 1] != '\n')
            printk("\n");
    }
    fs_unlock();
}

SHELL_CMD("fs", "cat", "Print a file", cmd_cat);

#endif // CONFIG_CMD_FS
