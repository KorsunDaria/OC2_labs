#include <stdio.h>
#include "mythread.h"

void *worker(void *arg)
{
    long id = (long)arg;
    printf("поток %ld: работаю\n", id);
    return (void *)(id * 10);
}

int main(void)
{
    mythread_t t1, t2;
    void *r1, *r2;

    if (mythread_create(&t1, worker, (void *)1) != 0) {
        perror("mythread_create t1");
        return 1;
    }
    if (mythread_create(&t2, worker, (void *)2) != 0) {
        perror("mythread_create t2");
        return 1;
    }

    if (mythread_join(t1, &r1) != 0) {
        perror("mythread_join t1");
        return 1;
    }
    if (mythread_join(t2, &r2) != 0) {
        perror("mythread_join t2");
        return 1;
    }

    printf("t1 вернул %ld, t2 вернул %ld\n", (long)r1, (long)r2);
    return 0;
}