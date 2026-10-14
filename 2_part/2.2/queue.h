#ifndef __FITOS_QUEUE_H__
#define __FITOS_QUEUE_H__

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct _QueueNode {
	int val;
	struct _QueueNode *next;
} qnode_t;

typedef struct _Queue {
	qnode_t *first;
	qnode_t *last;

	pthread_t qmonitor_tid;

	int count;
	int max_count;

	// synchronization primitives (each variant uses only its own)
	pthread_spinlock_t spin;                 // queue-spin.c
	pthread_mutex_t mutex;                   // queue-mutex.c, queue-cond.c
	pthread_cond_t not_empty, not_full;      // queue-cond.c
	sem_t lock, slots, items;                // queue-sem.c

	// queue statistics
	long add_attempts;
	long get_attempts;
	long add_count;
	long get_count;
} queue_t;

queue_t* queue_init(int max_count);
void queue_destroy(queue_t *q);
int queue_add(queue_t *q, int val);
int queue_get(queue_t *q, int *val);
void queue_print_stats(queue_t *q);

// common helpers (queue-common.c); caller must hold the queue lock
void queue_common_init(queue_t *q, int max_count);
void queue_common_destroy(queue_t *q);
void qlist_push(queue_t *q, int val);
int qlist_pop(queue_t *q);

#endif		// __FITOS_QUEUE_H__
