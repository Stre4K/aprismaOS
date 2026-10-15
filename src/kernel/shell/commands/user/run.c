// run.c
// <kernel/shell/commands.h>

#include <kernel/config.h>
#include <kernel/proc.h>

#if defined(CONFIG_CMD_USER) && defined(CONFIG_USERMODE)

#include <kernel/shell/commands.h>
#include <kernel/printk.h>

void cmd_run(int argc, char **argv) {
    if (argc < 2) {
        printk("Usage: run <program>   (see 'programs')\n");
        return;
    }

    process_t *p = process_spawn(argv[1]);
    if (p == NULL) {
        if (user_program_find(argv[1]))
            printk("run: not enough memory to start %s\n", argv[1]);
        else
            printk("run: no program named %s\n", argv[1]);
        return;
    }

    int pid = p->pid;
    bool killed;
    int code = process_wait_info(p, &killed);  // p is released afterwards
    if (killed)
        printk("[%d %s] was killed\n", pid, argv[1]);
    else
        printk("[%d %s] exited with code %d\n", pid, argv[1], code);
}

void cmd_programs(int argc, char **argv) {
    (void)argc;
    (void)argv;
    for (const user_program_t *p = user_programs; p->name; p++)
        printk("%-12s %u bytes\n", p->name, (unsigned)(p->end - p->start));
}

SHELL_CMD("user", "run", "Run a user program", cmd_run);
SHELL_CMD("user", "programs", "List the built-in user programs", cmd_programs);

#endif // CONFIG_CMD_USER && CONFIG_USERMODE
