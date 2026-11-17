#include "common.h"
#include "pqueue.h"

typedef struct qnode
{
	void *val;
	struct qnode *next;
} qnode_t;

struct pqueue
{
	qnode_t *first, *last;
	int count, max_count, closed;
	pthread_mutex_t mutex;
	pthread_cond_t not_empty;
};

pqueue_t *pqueue_create(int max_count)
{
	pqueue_t *q = calloc(1, sizeof(*q));
	if (!q)
		return NULL;
	q->max_count = max_count;
	if (pthread_mutex_init(&q->mutex, NULL) != 0)
	{
		free(q);
		return NULL;
	}
	if (pthread_cond_init(&q->not_empty, NULL) != 0)
	{
		pthread_mutex_destroy(&q->mutex);
		free(q);
		return NULL;
	}
	return q;
}

void pqueue_destroy(pqueue_t *q)
{
	if (!q)
		return;
	qnode_t *cur = q->first;
	while (cur)
	{
		qnode_t *next = cur->next;
		free(cur);
		cur = next;
	}
	CHK(pthread_cond_destroy(&q->not_empty));
	CHK(pthread_mutex_destroy(&q->mutex));
	free(q);
}

int pqueue_try_add(pqueue_t *q, void *val)
{
	qnode_t *n = malloc(sizeof(*n));
	if (!n)
		return -1;
	n->val = val;
	n->next = NULL;

	LOCK(&q->mutex);
	if (q->max_count > 0 && q->count >= q->max_count)
	{
		UNLOCK(&q->mutex);
		free(n);
		return 0;
	}
	if (q->last)
		q->last->next = n;
	else
		q->first = n;
	q->last = n;
	q->count++;
	// будим читателя
	CHK(pthread_cond_signal(&q->not_empty));
	UNLOCK(&q->mutex);
	return 1;
}

static void pop_locked(pqueue_t *q, void **val)
{
	qnode_t *n = q->first;
	*val = n->val;
	q->first = n->next;
	if (!q->first)
		q->last = NULL;
	q->count--;
	free(n);
}

int pqueue_try_get(pqueue_t *q, void **val)
{
	int ok = 0;
	LOCK(&q->mutex);
	if (q->count > 0)
	{
		pop_locked(q, val);
		ok = 1;
	}
	UNLOCK(&q->mutex);
	return ok;
}

int pqueue_get(pqueue_t *q, void **val)
{
	int ok = 0;
	LOCK(&q->mutex);
	// ждем элемент
	while (q->count == 0 && !q->closed)
		CHK(pthread_cond_wait(&q->not_empty, &q->mutex));
	if (q->count > 0)
	{
		pop_locked(q, val);
		ok = 1;
	}
	UNLOCK(&q->mutex);
	return ok;
}

void pqueue_close(pqueue_t *q)
{
	LOCK(&q->mutex);
	// закрываем и будим всех
	q->closed = 1;
	CHK(pthread_cond_broadcast(&q->not_empty));
	UNLOCK(&q->mutex);
}
