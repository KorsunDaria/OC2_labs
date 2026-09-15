#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

void *f(void *a)
{
    printf("arg = %p\n", a);
    while (1)
    {
        printf("!\n");
        sleep(1);
    }
    return 0;
}
int main()
{
    pthread_t t;
    pthread_create(&t, 0, f, 0);
    sleep(3);
    pthread_cancel(t);
    pthread_join(t, 0);
}