#define _GNU_SOURCE
#include "queue.h"

queue_t* queue_init(int max_count) {
	queue_t *q = malloc(sizeof(queue_t));
	if (!q) {
		printf("Cannot allocate memory for a queue\n");
		abort();
	}
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->not_empty, NULL);
	pthread_cond_init(&q->not_full, NULL);
	queue_common_init(q, max_count);
	return q;
}

void queue_destroy(queue_t *q) {
	queue_common_destroy(q);
	pthread_cond_destroy(&q->not_empty);
	pthread_cond_destroy(&q->not_full);
	pthread_mutex_destroy(&q->mutex);
	free(q);
}

int queue_add(queue_t *q, int val) {
	pthread_mutex_lock(&q->mutex);
	q->add_attempts++;
	while (q->count == q->max_count)
		pthread_cond_wait(&q->not_full, &q->mutex);
	qlist_push(q, val);
	pthread_cond_signal(&q->not_empty);
	pthread_mutex_unlock(&q->mutex);
	return 1;
}

int queue_get(queue_t *q, int *val) {
	pthread_mutex_lock(&q->mutex);
	q->get_attempts++;
	while (q->count == 0)
		pthread_cond_wait(&q->not_empty, &q->mutex);
	*val = qlist_pop(q);
	pthread_cond_signal(&q->not_full);
	pthread_mutex_unlock(&q->mutex);
	return 1;
}
