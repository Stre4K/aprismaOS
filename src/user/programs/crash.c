// crash: reading kernel memory must kill this process, not the kernel
#include "ulib.h"

int main(void)
{
    print("crash: reading kernel memory\n");
    volatile const char *kernel = (const char *)0xFFFFFFFF80100000UL;
    return *kernel;  // page fault in user mode
}
