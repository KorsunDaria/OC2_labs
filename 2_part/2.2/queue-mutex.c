#define _GNU_SOURCE
#include "queue.h"

queue_t* queue_init(int max_count) {
	queue_t *q = malloc(sizeof(queue_t));
	if (!q) {
		printf("Cannot allocate memory for a queue\n");
		abort();
	}
	pthread_mutex_init(&q->mutex, NULL);
	queue_common_init(q, max_count);
	return q;
}

void queue_destroy(queue_t *q) {
	queue_common_destroy(q);
	pthread_mutex_destroy(&q->mutex);
	free(q);
}

int queue_add(queue_t *q, int val) {
	pthread_mutex_lock(&q->mutex);
	q->add_attempts++;
	if (q->count == q->max_count) {
		pthread_mutex_unlock(&q->mutex);
		return 0;
	}
	qlist_push(q, val);
	pthread_mutex_unlock(&q->mutex);
	return 1;
}

int queue_get(queue_t *q, int *val) {
	pthread_mutex_lock(&q->mutex);
	q->get_attempts++;
	if (q->count == 0) {
		pthread_mutex_unlock(&q->mutex);
		return 0;
	}
	*val = qlist_pop(q);
	pthread_mutex_unlock(&q->mutex);
	return 1;
}
