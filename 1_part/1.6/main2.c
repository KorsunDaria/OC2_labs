#include <stdio.h>
#include "mythread.h"

static mythread_t t1;
static volatile int go = 0;
static volatile int res[3];

void *worker(void *arg)
{
    (void)arg;
    return (void *)10;
}

void *joiner(void *arg)
{
    long id = (long)arg;
    while (!go)
        ;
    res[id] = (mythread_join(t1, NULL) == 0);
    return NULL;
}

int main(void)
{
    mythread_t j1, j2;

    mythread_create(&t1, worker, NULL);
    mythread_create(&j1, joiner, (void *)1);
    mythread_create(&j2, joiner, (void *)2);

    go = 1;

    mythread_join(j1, NULL);
    mythread_join(j2, NULL);

    printf("joiner 1: %s\n", res[1] ? "выиграл" : "проиграл");
    printf("joiner 2: %s\n", res[2] ? "выиграл" : "проиграл");
    return 0;
}