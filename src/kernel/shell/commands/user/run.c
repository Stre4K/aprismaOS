// run.c
// <kernel/shell/commands.h>

#include <kernel/config.h>
#include <kernel/proc.h>

#if defined(CONFIG_CMD_USER) && defined(CONFIG_USERMODE)

#include <kernel/shell/commands.h>
#include <kernel/printk.h>
#include <kernel/console.h>
#include <kernel/fs.h>

void cmd_run(int argc, char **argv) {
    if (argc < 2) {
        printk("Usage: run <program> [arguments...]   (see 'programs')\n");
        return;
    }

    // The program gets "<program> [arguments...]" as its argv
    process_t *p = process_spawn_args(argv[1], argc - 1, (const char *const *)&argv[1], NULL);
    if (p == NULL) {
        if (!fs_mounted())
            printk("run: no filesystem (boot with a ramdisk)\n");
        else if (process_find_program(argv[1]))
            printk("run: cannot start %s (not a valid program, or out of memory)\n", argv[1]);
        else
            printk("run: no program named %s\n", argv[1]);
        return;
    }

    int pid = p->pid;
    bool killed;
    console_set_foreground(p->pgid);  // Ctrl+C now ends this program
    int code = process_wait_info(p, &killed);  // p is released afterwards
    console_set_foreground(0);
    if (killed)
        printk("[%d %s] was killed\n", pid, argv[1]);
    else
        printk("[%d %s] exited with code %d\n", pid, argv[1], code);
}

void cmd_programs(int argc, char **argv) {
    (void)argc;
    (void)argv;
    const fs_node_t *node;
    const char *name;
    for (size_t i = 0; (node = fs_list("/bin", i, &name)) != NULL; i++) {
        if (!node->is_dir)
            printk("%-12s %u bytes\n", name, (unsigned)node->size);
    }
}

SHELL_CMD("user", "run", "Run a user program", cmd_run);
SHELL_CMD("user", "programs", "List the programs in /bin", cmd_programs);

#endif // CONFIG_CMD_USER && CONFIG_USERMODE
