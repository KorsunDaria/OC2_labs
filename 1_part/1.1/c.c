#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#define NTHREADS 5

int global_var = 100;

pthread_t tid[NTHREADS];
pthread_t self_ids[NTHREADS];

void *mythread(void *arg)
{
	pthread_t *self_slot = (pthread_t *)arg;
	int local_var = 1;
	static int local_static_var = 10;
	const int local_const_var = 20;

	*self_slot = pthread_self();

	printf("mythread [%d %d %d]: Hello from mythread!\n",
		getpid(), getppid(), gettid());

	printf("&local_var = %p, &local_static_var = %p, &local_const_var = %p, &global_var = %p\n",
		(void *)&local_var, (void *)&local_static_var,
		(void *)&local_const_var, (void *)&global_var);
    sleep(10);
	return NULL;
}

int main(void)
{
	int err, i;

	printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());

	for (i = 0; i < NTHREADS; i++) {
		err = pthread_create(&tid[i], NULL, mythread, &self_ids[i]);
		if (err) {
			printf("main: pthread_create() failed: %s\n", strerror(err));
			return -1;
		}
	}

	for (i = 0; i < NTHREADS; i++) {
		pthread_join(tid[i], NULL);
		printf("thread %d: pthread_self() = %lu, from pthread_create() = %lu, pthread_equal() = %d\n",
			i, (unsigned long)self_ids[i], (unsigned long)tid[i],
			pthread_equal(self_ids[i], tid[i]));
	}

	return 0;
}

/*
./c &
PID=$!
sleep 1
cat /proc/$PID/maps
*/