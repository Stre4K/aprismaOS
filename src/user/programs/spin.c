// spin: busy loop without system calls. Only the timer interrupt can take
// the CPU away from it, so other threads running meanwhile prove that user
// code is preemptible.
int main(void)
{
    for (volatile unsigned long i = 0; i < 100000000UL; i++)
        ;
    return 0;
}
