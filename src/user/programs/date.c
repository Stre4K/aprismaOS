// date: show the date and time (UTC, from the real-time clock); with -s
// the seconds since 1970 instead
#include "ulib.h"

static const char *const weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

int main(int argc, char **argv)
{
    long now = time();
    if (argc > 1 && strcmp(argv[1], "-s") == 0) {
        print_int(now);
        print("\n");
        return 0;
    }
    if (argc > 1) {
        eprint("usage: date [-s]\n");
        return 1;
    }
    date_t d;
    time_to_date(now, &d);
    char text[20];
    format_time(text, now);
    print(weekdays[d.weekday]);
    print(" ");
    print(text);
    print(" UTC\n");
    return 0;
}
