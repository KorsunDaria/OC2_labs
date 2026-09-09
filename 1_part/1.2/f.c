#include <stdio.h>
#include <pthread.h>
#include <string.h>

void *f(void *a)
{
    printf("arg = %p\n", a);
    printf("%lu\n", (unsigned long)pthread_self());
    return NULL;
}

int main()
{
    pthread_t t;
    pthread_attr_t at;
    long n = 0;
    int rc;

    rc = pthread_attr_init(&at);
    if (rc != 0)
    {
        fprintf(stderr, "pthread_attr_init : %s\n", strerror(rc));
        return 1;
    }

    rc = pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    if (rc != 0)
    {
        fprintf(stderr, "pthread_attr_setdetachstate : %s\n", strerror(rc));
        pthread_attr_destroy(&at);
        return 1;
    }
    while (1)
    {
        if (pthread_create(&t, &at, f, NULL) != 0)
        {
            printf("%ld\n", n);
            break;
        }
        n++;
    }
    return 0;
}























// ps -o pid,vsize,comm -p \$(pgrep my_f)"