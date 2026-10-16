// forever: loop without system calls until Ctrl+C kills it
int main(void)
{
    for (volatile unsigned long i = 0;; i++)
        ;
}
