// args: print the arguments; the exit code is argc. Also checks what the
// kernel must guarantee at entry (exit codes 100+ mean a broken layout).
#include "ulib.h"
#include <stdint.h>

int main(int argc, char **argv)
{
    if (argv[argc] != 0)
        return 100;  // argv must end with a NULL pointer
    if (((uintptr_t)argv - 8) % 16 != 0)
        return 101;  // argc sits at a 16-byte aligned stack pointer

    print("argc=");
    print_int(argc);
    print("\n");
    for (int i = 0; i < argc; i++) {
        print("argv[");
        print_int(i);
        print("]=");
        print(argv[i]);
        print("\n");
    }
    return argc;
}
