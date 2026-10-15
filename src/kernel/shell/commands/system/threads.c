// threads.c
// <kernel/shell/commands.h>

#include <kernel/config.h>
#include <kernel/sched.h>

#if defined(CONFIG_CMD_SYSTEM) && defined(CONFIG_SCHED)

#include <kernel/shell/commands.h>
#include <kernel/arch/timer.h>
#include <kernel/printk.h>

#define MAX_LISTED 64

void cmd_threads(int argc, char **argv) {
    (void)argc;
    (void)argv;

    static thread_info_t threads[MAX_LISTED];
    size_t count = thread_list(threads, MAX_LISTED);
    size_t shown = count < MAX_LISTED ? count : MAX_LISTED;

    printk("  ID  NAME             STATE      CPU (ms)\n");
    for (size_t i = 0; i < shown; i++) {
        printk("%4d  %-16s %-10s %llu\n", threads[i].id, threads[i].name,
               thread_state_name(threads[i].state),
               (unsigned long long)(threads[i].ticks * 1000 / PIT_FREQUENCY));
    }
    if (count > shown)
        printk("  ... %u more\n", (unsigned)(count - shown));
}

SHELL_CMD("system", "threads", "List kernel threads", cmd_threads);

#endif // CONFIG_CMD_SYSTEM && CONFIG_SCHED
