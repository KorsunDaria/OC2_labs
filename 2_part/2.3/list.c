
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>


#define CHK(call) do { int e_ = (call); if (e_) { \
	fprintf(stderr, "%s:%d: %s failed: %s\n", __FILE__, __LINE__, #call, strerror(e_)); \
	exit(EXIT_FAILURE); } } while (0)


#if defined(USE_MUTEX)
typedef pthread_mutex_t lock_t;
#define NAME "mutex"
static int l_init(lock_t *l)    { return pthread_mutex_init(l, NULL); }
static int l_destroy(lock_t *l) { return pthread_mutex_destroy(l); }
static int l_rd(lock_t *l)      { return pthread_mutex_lock(l); }
static int l_wr(lock_t *l)      { return pthread_mutex_lock(l); }
static int l_un(lock_t *l)      { return pthread_mutex_unlock(l); }
#elif defined(USE_SPIN)
typedef pthread_spinlock_t lock_t;
#define NAME "spin"
static int l_init(lock_t *l)    { return pthread_spin_init(l, PTHREAD_PROCESS_PRIVATE); }
static int l_destroy(lock_t *l) { return pthread_spin_destroy(l); }
static int l_rd(lock_t *l)      { return pthread_spin_lock(l); }
static int l_wr(lock_t *l)      { return pthread_spin_lock(l); }
static int l_un(lock_t *l)      { return pthread_spin_unlock(l); }
#elif defined(USE_RWLOCK)
typedef pthread_rwlock_t lock_t;
#define NAME "rwlock"
static int l_init(lock_t *l)    { return pthread_rwlock_init(l, NULL); }
static int l_destroy(lock_t *l) { return pthread_rwlock_destroy(l); }
static int l_rd(lock_t *l)      { return pthread_rwlock_rdlock(l); }
static int l_wr(lock_t *l)      { return pthread_rwlock_wrlock(l); }
static int l_un(lock_t *l)      { return pthread_rwlock_unlock(l); }
#else
#error "define USE_MUTEX, USE_SPIN or USE_RWLOCK"
#endif

typedef struct _Node {
	char value[100];
	struct _Node *next;
	lock_t sync;
} Node;

typedef struct _Storage {
	Node *first;   
	               
} Storage;

#define NSCAN 3
#define NSWAP 3

static Storage st;
static size_t n;                         
static int stop;                        
static long long iters[NSCAN];           
static long long pairs_last[NSCAN];      
static long long swaps[NSWAP];          

static int asc(size_t a, size_t b)  { return a < b; }
static int desc(size_t a, size_t b) { return a > b; }
static int eq(size_t a, size_t b)   { return a == b; }
static int (*const cmps[NSCAN])(size_t, size_t) = { asc, desc, eq };
static const char *const cmp_names[NSCAN] = { "asc ", "desc", "eq  " };

static Node *node_new(const char *s) {
	Node *x = malloc(sizeof(Node));
	if (!x) { fprintf(stderr, "malloc failed\n"); exit(EXIT_FAILURE); }
	strncpy(x->value, s, sizeof(x->value) - 1);
	x->value[sizeof(x->value) - 1] = '\0';
	x->next = NULL;
	CHK(l_init(&x->sync));
	return x;
}

static void storage_fill(void) {
	unsigned seed = (unsigned)time(NULL);
	char buf[100];
	st.first = node_new("");
	for (size_t i = 0; i < n; i++) {
		int len = 1 + rand_r(&seed) % 99;          
		memset(buf, 'a', len);
		buf[len] = '\0';
		Node *x = node_new(buf);
		x->next = st.first->next;
		st.first->next = x;
	}
}

static void storage_free(void) {
	Node *c = st.first;
	while (c) {
		Node *nx = c->next;
		CHK(l_destroy(&c->sync));
		free(c);
		c = nx;
	}
}

static void *scanner(void *arg) {
	int id = (int)(intptr_t)arg;
	while (!__atomic_load_n(&stop, __ATOMIC_RELAXED)) {
		long long pairs = 0;
		Node *prev = st.first;
		CHK(l_rd(&prev->sync));
		Node *a = prev->next;
		if (a)
			CHK(l_rd(&a->sync));
		CHK(l_un(&prev->sync));
		while (a) {
			Node *b = a->next;         
			if (!b)
				break;
			CHK(l_rd(&b->sync));
			if (cmps[id](strlen(a->value), strlen(b->value)))
				pairs++;
			CHK(l_un(&a->sync));
			a = b;
		}
		if (a)
			CHK(l_un(&a->sync));
		pairs_last[id] = pairs;
		__atomic_fetch_add(&iters[id], 1, __ATOMIC_RELAXED);
	}
	return NULL;
}


static void *swapper(void *arg) {
	int id = (int)(intptr_t)arg;
	unsigned seed = (unsigned)time(NULL) ^ (unsigned)(id * 2654435761u);
	while (!__atomic_load_n(&stop, __ATOMIC_RELAXED)) {
		if (rand_r(&seed) % 2)         
			continue;
		size_t pos = rand_r(&seed) % n;

		Node *p = st.first;
		CHK(l_wr(&p->sync));
		for (size_t i = 0; i < pos; i++) {
			Node *nx = p->next;
			if (!nx)
				break;
			CHK(l_wr(&nx->sync));
			CHK(l_un(&p->sync));
			p = nx;
		}
		Node *a = p->next;
		if (!a) {
			CHK(l_un(&p->sync));
			continue;
		}
		CHK(l_wr(&a->sync));
		Node *b = a->next;
		if (!b) {
			CHK(l_un(&a->sync));
			CHK(l_un(&p->sync));
			continue;
		}
		CHK(l_wr(&b->sync));

		p->next = b;
		a->next = b->next;
		b->next = a;

		CHK(l_un(&b->sync));
		CHK(l_un(&a->sync));
		CHK(l_un(&p->sync));
		__atomic_fetch_add(&swaps[id], 1, __ATOMIC_RELAXED);
	}
	return NULL;
}

static void print_stats(void) {
	for (int i = 0; i < NSCAN; i++)
		printf("scan %s: iters=%lld pairs(last)=%lld | ", cmp_names[i],
		       __atomic_load_n(&iters[i], __ATOMIC_RELAXED), pairs_last[i]);
	printf("swaps=(%lld %lld %lld)\n",
	       __atomic_load_n(&swaps[0], __ATOMIC_RELAXED),
	       __atomic_load_n(&swaps[1], __ATOMIC_RELAXED),
	       __atomic_load_n(&swaps[2], __ATOMIC_RELAXED));
}

int main(int argc, char **argv) {
	long len = 1000, secs = 5;
	char *end;

	if (argc > 1) {
		len = strtol(argv[1], &end, 10);
		if (*end || len < 3) { fprintf(stderr, "usage: %s <len>=3+> [seconds]\n", argv[0]); return EXIT_FAILURE; }
	}
	if (argc > 2) {
		secs = strtol(argv[2], &end, 10);
		if (*end || secs < 1) { fprintf(stderr, "usage: %s <len> [seconds>=1]\n", argv[0]); return EXIT_FAILURE; }
	}
	n = (size_t)len;
	printf("sync=%s len=%zu time=%lds\n", NAME, n, secs);

	storage_fill();

	pthread_t sc[NSCAN], sw[NSWAP];
	for (intptr_t i = 0; i < NSCAN; i++)
		CHK(pthread_create(&sc[i], NULL, scanner, (void *)i));
	for (intptr_t i = 0; i < NSWAP; i++)
		CHK(pthread_create(&sw[i], NULL, swapper, (void *)i));

	for (long s = 0; s < secs; s++) {
		sleep(1);
		print_stats();
	}

	__atomic_store_n(&stop, 1, __ATOMIC_RELAXED);
	for (int i = 0; i < NSCAN; i++)
		CHK(pthread_join(sc[i], NULL));
	for (int i = 0; i < NSWAP; i++)
		CHK(pthread_join(sw[i], NULL));

	printf("FINAL: ");
	print_stats();
	storage_free();
	return EXIT_SUCCESS;
}
