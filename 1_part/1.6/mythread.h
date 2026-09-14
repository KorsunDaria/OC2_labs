#ifndef MYTHREAD_H
#define MYTHREAD_H

#include <sys/types.h>

typedef pid_t mythread_t;
int mythread_create(mythread_t *thread, void *(*start_routine)(void *), void *arg);

int mythread_join(mythread_t thread, void **retval);

#endif /* MYTHREAD_H */