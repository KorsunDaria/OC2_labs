#define _GNU_SOURCE
#include "queue.h"

static void *qmonitor(void *arg) {
	queue_t *q = (queue_t *)arg;

	printf("qmonitor: [%d %d %d]\n", getpid(), getppid(), gettid());

	while (1) {
		queue_print_stats(q);
		sleep(1);
	}

	return NULL;
}

void queue_common_init(queue_t *q, int max_count) {
	int err;

	q->first = q->last = NULL;
	q->max_count = max_count;
	q->count = 0;
	q->add_attempts = q->get_attempts = 0;
	q->add_count = q->get_count = 0;

	err = pthread_create(&q->qmonitor_tid, NULL, qmonitor, q);
	if (err) {
		printf("queue_init: pthread_create() failed: %s\n", strerror(err));
		abort();
	}
}

void queue_common_destroy(queue_t *q) {
	int err;

	err = pthread_cancel(q->qmonitor_tid);
	if (err) {
		printf("queue_destroy: pthread_cancel() failed: %s\n", strerror(err));
		abort();
	}
	err = pthread_join(q->qmonitor_tid, NULL);
	if (err) {
		printf("queue_destroy: pthread_join() failed: %s\n", strerror(err));
		abort();
	}

	qnode_t *cur = q->first;
	while (cur) {
		qnode_t *next = cur->next;
		free(cur);
		cur = next;
	}
}

// the queue must not be full
void qlist_push(queue_t *q, int val) {
	qnode_t *new = malloc(sizeof(qnode_t));
	if (!new) {
		printf("Cannot allocate memory for new node\n");
		abort();
	}
	new->val = val;
	new->next = NULL;

	if (!q->first)
		q->first = q->last = new;
	else {
		q->last->next = new;
		q->last = new;
	}
	q->count++;
	q->add_count++;
}

// the queue must not be empty
int qlist_pop(queue_t *q) {
	qnode_t *tmp = q->first;
	int val = tmp->val;

	q->first = tmp->next;
	free(tmp);
	q->count--;
	q->get_count++;
	return val;
}

void queue_print_stats(queue_t *q) {
	printf("queue stats: current size %d; attempts: (%ld %ld %ld); counts (%ld %ld %ld)\n",
		q->count,
		q->add_attempts, q->get_attempts, q->add_attempts - q->get_attempts,
		q->add_count, q->get_count, q->add_count - q->get_count);
}
