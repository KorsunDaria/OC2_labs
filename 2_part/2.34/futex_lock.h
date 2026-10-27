
#ifndef FUTEX_LOCK_H
#define FUTEX_LOCK_H
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <linux/futex.h>
#include <sys/syscall.h>

static inline void fl_die(const char *what) {
	fprintf(stderr, "%s failed: %s\n", what, strerror(errno));
	exit(EXIT_FAILURE);
}

typedef struct { int v; } cas_spinlock_t;

static inline void spin_lock(cas_spinlock_t *l) {
	while (__sync_val_compare_and_swap(&l->v, 0, 1) != 0)
		;
}
static inline void spin_unlock(cas_spinlock_t *l) {
	__atomic_store_n(&l->v, 0, __ATOMIC_RELEASE);
}

typedef struct { int v; } futex_mutex_t;

static inline void futex_wait(int *addr, int val) {
	if (syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0) == -1
	    && errno != EAGAIN && errno != EINTR)
		fl_die("futex(FUTEX_WAIT)");
}
static inline void futex_wake(int *addr, int n) {
	if (syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, n, NULL, NULL, 0) == -1)
		fl_die("futex(FUTEX_WAKE)");
}

static inline void mutex_lock(futex_mutex_t *m) {
	int c = __sync_val_compare_and_swap(&m->v, 0, 1);
	if (c == 0)
		return;
	do {
		if (c == 2 || __sync_val_compare_and_swap(&m->v, 1, 2) != 0)
			futex_wait(&m->v, 2);
	} while ((c = __sync_val_compare_and_swap(&m->v, 0, 2)) != 0);
}

static inline void mutex_unlock(futex_mutex_t *m) {
	if (__sync_fetch_and_sub(&m->v, 1) != 1) {
		__atomic_store_n(&m->v, 0, __ATOMIC_RELEASE);
		futex_wake(&m->v, 1);
	}
}
#endif