// cat.c
// <kernel/shell/commands.h>

#include <kernel/config.h>

#ifdef CONFIG_CMD_FS

#include <kernel/shell/commands.h>
#include <kernel/fs.h>
#include <kernel/printk.h>
#include <kernel/abi/syscall.h>

void cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        printk("Usage: cat <file>\n");
        return;
    }

    fs_node_t *node;
    int r = fs_open(argv[1], O_RDONLY, &node);  // kept while it is printed
    if (r == -EISDIR) {
        printk("cat: %s: is a directory\n", argv[1]);
        return;
    }
    if (r < 0) {
        printk("cat: %s: %s\n", argv[1], r == -EIO ? "cannot be read" : "no such file");
        return;
    }

    char buf[256];
    size_t offset = 0;
    long n;
    char last = '\n';
    while ((n = fs_read(node, offset, buf, sizeof(buf))) > 0) {
        printk("%.*s", (int)n, buf);
        last = buf[n - 1];
        offset += (size_t)n;
    }
    if (n < 0)
        printk("\ncat: %s: cannot be read\n", argv[1]);
    else if (last != '\n')
        printk("\n");
    fs_close(node);
}

SHELL_CMD("fs", "cat", "Print a file", cmd_cat);

#endif // CONFIG_CMD_FS
