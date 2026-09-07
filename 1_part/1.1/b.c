#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#define NTHREADS 5

void *mythread(void *arg) {
    printf("arg = %p\n", arg);
	printf("mythread [%d %d %d]: Hello from mythread!\n", getpid(), getppid(), gettid());
	return NULL;
}

int main() {
	pthread_t tid[NTHREADS];
	int err, i;

	printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());

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

