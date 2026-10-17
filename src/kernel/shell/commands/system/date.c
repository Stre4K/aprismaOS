// date.c
// <kernel/shell/commands.h>

#include <kernel/config.h>

#ifdef CONFIG_CMD_SYSTEM

#include <kernel/shell/commands.h>
#include <kernel/printk.h>
#include <kernel/time.h>

static const char *const weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

void cmd_date(int argc, char **argv) {
    (void)argc;
    (void)argv;
    date_t d;
    time_to_date((long long)time_now(), &d);
    printk("%s %04d-%02d-%02d %02d:%02d:%02d UTC\n", weekdays[d.weekday], d.year, d.month, d.day,
           d.hour, d.minute, d.second);
}

SHELL_CMD("system", "date", "Show the date and time (UTC)", cmd_date);

#endif // CONFIG_CMD_SYSTEM
