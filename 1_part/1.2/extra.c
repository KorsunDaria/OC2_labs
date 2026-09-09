#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

void *f(void *a)
{
    printf("arg = %p\n", a);
    printf("tid = %lu\n", (unsigned long)pthread_self());
    return NULL;
}

int main()
{
    pthread_t t;
    pthread_attr_t at;

    pthread_attr_init(&at);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);

    pthread_create(&t, &at, f, NULL);

    int rc = pthread_join(t, NULL);
    if (rc != 0)
    {
        printf("error: %s\n", strerror(rc));
    }
    else
    {
        printf("joined \n");
    }

    pthread_attr_destroy(&at);
    return 0;
}