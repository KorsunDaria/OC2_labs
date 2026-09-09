#include <stdio.h>
#include <pthread.h>

void *f(void *a)
{
    printf("arg = %p\n", a);
    printf("%lu\n", (unsigned long) pthread_self());
    return NULL;
}

int main()
{
    pthread_t t;
    long n = 0;
    while (1)
    {
        if (pthread_create(&t, NULL, f, NULL) != 0)
        {
            printf("%ld\n", n);
            break;
        }
        n++;
    }
    return 0;
}


























// watch -n 0.2 "ps -o pid,vsize,comm -p \$(pgrep my_d)"