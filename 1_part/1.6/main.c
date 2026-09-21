#include <stdio.h>
#include <unistd.h>
#include "mythread.h"

mythread_t t1;   

void *worker(void *arg)
{
    long id = (long)arg;
    printf("поток %ld: работаю, пытаюсь заджойнить t1\n", id);

    void *r2;
    if (mythread_join(t1, &r2) != 0) {
        perror("worker: mythread_join(t1)");
        return (void *)1;
    }
    printf("поток %ld: увидел, что t1 вернул %ld\n", id, (long)r2);
    return (void *)(id * 10);
}

void *slepp_wrap(void *arg)
{
    printf("arg = %p\n", arg);
    sleep(3);
    return NULL;
}

int main(void)
{
    mythread_t t2;
    void *r1, *r2;

    if (mythread_create(&t1, slepp_wrap, (void *)1) != 0) {
        perror("mythread_create t1");
        return 1;
    }
    if (mythread_create(&t2, worker, (void *)2) != 0) {
        perror("mythread_create t2");
        return 1;
    }

    printf("main: пытаюсь заджойнить t1\n");
    if (mythread_join(t1, &r1) != 0) {
        perror("main: mythread_join(t1)");
    } else {
        printf("main: увидел, что t1 вернул %ld\n", (long)r1);
    }

    if (mythread_join(t2, &r2) != 0) {
        perror("mythread_join t2");
        return 1;
    }
    printf("t2 вернул %ld\n", (long)r2);
    return 0;
}