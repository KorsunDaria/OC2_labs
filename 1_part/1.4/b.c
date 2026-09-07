
#include <pthread.h>
#include <unistd.h>

void *f(void *a)
{
    long c = 0;
    while (1)
    {
        c++;
        pthread_testcancel();
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