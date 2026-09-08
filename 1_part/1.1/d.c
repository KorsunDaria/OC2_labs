#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#define NTHREADS 5

int global_var = 100;

void *mythread(void *arg)
{
	int thread_num = *(int *)arg;
	int local_var = 1;

	printf("thread %d [tid=%d]: local_var = %d, global_var = %d\n",
		thread_num, gettid(), local_var, global_var);

	local_var = local_var + 1;
	global_var = global_var + 1;

	printf("thread %d [tid=%d]: local_var = %d, global_var = %d\n",
		thread_num, gettid(), local_var, global_var);

	return NULL;
}

int main(void)
{
	pthread_t tid[NTHREADS];
	int thread_nums[NTHREADS];
	int err, i;

	for (i = 0; i < NTHREADS; i++) {
		thread_nums[i] = i + 1;
		err = pthread_create(&tid[i], NULL, mythread, &thread_nums[i]);
		if (err) {
			printf("main: pthread_create() failed: %s\n", strerror(err));
			return -1;
		}
	}

	for (i = 0; i < NTHREADS; i++)
		pthread_join(tid[i], NULL);

	return 0;
}
