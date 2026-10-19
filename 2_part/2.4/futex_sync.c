
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/futex.h>
#include <sys/syscall.h>

#define CHK(call) do { int e_ = (call); if (e_) { \
	fprintf(stderr, "%s:%d: %s failed: %s\n", __FILE__, __LINE__, #call, strerror(e_)); \
	exit(EXIT_FAILURE); } } while (0)

static void die(const char *what) {
	fprintf(stderr, "%s failed: %s\n", what, strerror(errno));
	exit(EXIT_FAILURE);
}


typedef struct { int v; } cas_spinlock_t;

static void spin_lock(cas_spinlock_t *l) {
	while (__sync_val_compare_and_swap(&l->v, 0, 1) != 0)
		;
}
static void spin_unlock(cas_spinlock_t *l) {
	__atomic_store_n(&l->v, 0, __ATOMIC_RELEASE);
}

typedef struct { int v; } futex_mutex_t;

static void futex_wait(int *addr, int val) {
	if (syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0) == -1
	    && errno != EAGAIN && errno != EINTR)
		die("futex(FUTEX_WAIT)");
}
static void futex_wake(int *addr, int n) {
	if (syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, n, NULL, NULL, 0) == -1)
		die("futex(FUTEX_WAKE)");
}

static void mutex_lock(futex_mutex_t *m) {
	int c = __sync_val_compare_and_swap(&m->v, 0, 1);
	if (c == 0)
		return;                                   
	do {
		
		if (c == 2 || __sync_val_compare_and_swap(&m->v, 1, 2) != 0)
			futex_wait(&m->v, 2);
		
	} while ((c = __sync_val_compare_and_swap(&m->v, 0, 2)) != 0);
}

static void mutex_unlock(futex_mutex_t *m) {
	if (__sync_fetch_and_sub(&m->v, 1) != 1) {    
		__atomic_store_n(&m->v, 0, __ATOMIC_RELEASE);
		futex_wake(&m->v, 1);
	}
}


enum { M_SPIN, M_MUTEX, M_NONE };

static int mode;
static long iters_per_thread = 1000000;
static volatile long counter;                             
static cas_spinlock_t sl;
static futex_mutex_t fm;

static void *worker(void *arg) {
	(void)arg;
	for (long i = 0; i < iters_per_thread; i++) {
		if (mode == M_SPIN)       spin_lock(&sl);
		else if (mode == M_MUTEX) mutex_lock(&fm);
		counter++;
		if (mode == M_SPIN)       spin_unlock(&sl);
		else if (mode == M_MUTEX) mutex_unlock(&fm);
	}
	return NULL;
}

int main(int argc, char **argv) {
	long nthreads = 4;
	char *end;

	if (argc < 2) goto usage;
	if (!strcmp(argv[1], "spin"))       mode = M_SPIN;
	else if (!strcmp(argv[1], "mutex")) mode = M_MUTEX;
	else if (!strcmp(argv[1], "none"))  mode = M_NONE;
	else goto usage;

	if (argc > 2) {
		nthreads = strtol(argv[2], &end, 10);
		if (*end || nthreads < 1 || nthreads > 256) goto usage;
	}
	if (argc > 3) {
		iters_per_thread = strtol(argv[3], &end, 10);
		if (*end || iters_per_thread < 1) goto usage;
	}

	pthread_t *t = malloc(sizeof(pthread_t) * nthreads);
	if (!t) die("malloc");

	struct timespec t0, t1;
	if (clock_gettime(CLOCK_MONOTONIC, &t0)) die("clock_gettime");
	for (long i = 0; i < nthreads; i++)
		CHK(pthread_create(&t[i], NULL, worker, NULL));
	for (long i = 0; i < nthreads; i++)
		CHK(pthread_join(t[i], NULL));
	if (clock_gettime(CLOCK_MONOTONIC, &t1)) die("clock_gettime");

	double dt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
	long expected = nthreads * iters_per_thread;
	printf("mode=%s threads=%ld counter=%ld expected=%ld %s time=%.3fs\n",
	       argv[1], nthreads, counter, expected,
	       counter == expected ? "OK" : "RACE!", dt);
	free(t);
	return counter == expected || mode == M_NONE ? EXIT_SUCCESS : EXIT_FAILURE;

usage:
	fprintf(stderr, "usage: %s <spin|mutex|none> [threads 1..256] [iters]\n", argv[0]);
	return EXIT_FAILURE;
}
