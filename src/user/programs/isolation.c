// isolation: run two copies at once. `owner` has the same virtual address
// in both, so if they shared memory one would see the other's pid.
#include "ulib.h"

static volatile long owner;

int main(void)
{
    long pid = getpid();
    owner = pid;

    for (int i = 0; i < 10; i++) {
        sleep(10);  // let the other copy run
        if (owner != pid) {
            print("isolation: FAILED\n");
            return 1;
        }
    }
    print("isolation: ok\n");
    return 0;
}
