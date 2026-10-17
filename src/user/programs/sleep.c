// sleep: wait the given number of seconds; decimals are fine ("sleep 0.5")
#include "ulib.h"

int main(int argc, char **argv)
{
    const char *s = argc == 2 ? argv[1] : "";
    unsigned long ms = 0;
    int digits = 0;
    for (; *s >= '0' && *s <= '9'; s++, digits++)
        ms = ms * 10 + (unsigned long)(*s - '0');
    ms *= 1000;
    if (*s == '.') {
        unsigned long scale = 100;
        for (s++; *s >= '0' && *s <= '9'; s++, digits++) {
            ms += (unsigned long)(*s - '0') * scale;
            scale /= 10;
        }
    }
    if (digits == 0 || *s != '\0' || ms > 0xFFFFFFFFUL) {
        eprint("usage: sleep <seconds>\n");
        return 1;
    }
    return sleep((unsigned)ms) < 0;  // -EINTR: killed (we end right after)
}
