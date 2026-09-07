#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#define NTHREADS 5

void *mythread(void *arg)
{
	(void)arg;
	printf("thread [tid=%d]: running, pid=%d\n", gettid(), getpid());
	sleep(15);
	return NULL;
}

int main(void)
{
	pthread_t tid[NTHREADS];
	int err, i;

	printf("main pid = %d\n", getpid());

	for (i = 0; i < NTHREADS; i++) {
		err = pthread_create(&tid[i], NULL, mythread, NULL);
		if (err) {
			printf("main: pthread_create() failed: %s\n", strerror(err));
			return -1;
		}
	}

	for (i = 0; i < NTHREADS; i++)
		pthread_join(tid[i], NULL);

	return 0;
}

/*
./e &
PID=$!
sleep 1
cat /proc/$PID/maps
*/