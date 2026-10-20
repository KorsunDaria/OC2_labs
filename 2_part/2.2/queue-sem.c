#define _GNU_SOURCE
#include "queue.h"

queue_t* queue_init(int max_count) {
	queue_t *q = malloc(sizeof(queue_t));
	if (!q) {
		printf("Cannot allocate memory for a queue\n");
		abort();
	}
	sem_init(&q->lock, 0, 1);          
	sem_init(&q->slots, 0, max_count); 
	sem_init(&q->items, 0, 0);         
	queue_common_init(q, max_count);
	return q;
}

void queue_destroy(queue_t *q) {
	queue_common_destroy(q);
	sem_destroy(&q->lock);
	sem_destroy(&q->slots);
	sem_destroy(&q->items);
	free(q);
}

// blocks while the queue is full
int queue_add(queue_t *q, int val) {
	sem_wait(&q->slots);
	sem_wait(&q->lock);
	q->add_attempts++;
	qlist_push(q, val);
	sem_post(&q->lock);
	sem_post(&q->items);
	return 1;
}

int queue_get(queue_t *q, int *val) {
	sem_wait(&q->items);
	sem_wait(&q->lock);
	q->get_attempts++;
	*val = qlist_pop(q);
	sem_post(&q->lock);
	sem_post(&q->slots);
	return 1;
}
