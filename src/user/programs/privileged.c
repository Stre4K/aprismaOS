// privileged: a ring 0 instruction must kill this process, not the kernel
#include "ulib.h"

int main(void)
{
    print("privileged: executing cli\n");
    __asm__ volatile("cli");  // general protection fault in user mode
    return 0;
}
