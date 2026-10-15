// hello: the smallest useful program
#include "ulib.h"

int main(void)
{
    print("Hello from user mode! (pid ");
    print_int(getpid());
    print(")\n");
    return 0;
}
