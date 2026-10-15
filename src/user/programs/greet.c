// greet: read a line from the keyboard
#include "ulib.h"

int main(void)
{
    char name[64];

    print("What's your name? ");
    long n = read(0, name, sizeof(name) - 1);
    if (n < 0)
        return 1;
    if (n > 0 && name[n - 1] == '\n')
        n--;  // read returns the line with its newline
    name[n] = '\0';

    print("Hello, ");
    print(n ? name : "stranger");
    print("!\n");
    return 0;
}
