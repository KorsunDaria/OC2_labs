#define _GNU_SOURCE
#include "queue.h"

//time timeout 10 ./queue-spin > /dev/null
//top -H
//timeout 30 stdbuf -oL ./queue-spin > out.txt

queue_t* queue_init(int max_count) {
	queue_t *q = malloc(sizeof(queue_t));
	if (!q) {
		printf("Cannot allocate memory for a queue\n");
		abort();
	}
	pthread_spin_init(&q->spin, PTHREAD_PROCESS_PRIVATE);
	queue_common_init(q, max_count);
	return q;
}

void queue_destroy(queue_t *q) {
	queue_common_destroy(q);
	pthread_spin_destroy(&q->spin);
	free(q);
}

int queue_add(queue_t *q, int val) {
	pthread_spin_lock(&q->spin);
	q->add_attempts++;
	if (q->count == q->max_count) {
		pthread_spin_unlock(&q->spin);
		return 0;
	}
	qlist_push(q, val);
	pthread_spin_unlock(&q->spin);
	return 1;
}

int queue_get(queue_t *q, int *val) {
	pthread_spin_lock(&q->spin);
	q->get_attempts++;
	if (q->count == 0) {
		pthread_spin_unlock(&q->spin);
		return 0;
	}
	*val = qlist_pop(q);
	pthread_spin_unlock(&q->spin);
	return 1;
}
