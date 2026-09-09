#include <stdio.h>
#include <pthread.h>

void *f(void *a)
{
    printf("arg = %p\n", a);
    pthread_detach(pthread_self());
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



//ps -o pid,vsize,comm -p \$(pgrep my_e)"