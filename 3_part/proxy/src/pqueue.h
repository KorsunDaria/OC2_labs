#ifndef PROXY_PQUEUE_H
#define PROXY_PQUEUE_H

typedef struct pqueue pqueue_t;

pqueue_t *pqueue_create(int max_count);
void pqueue_destroy(pqueue_t *q);
int pqueue_try_add(pqueue_t *q, void *val);
int pqueue_try_get(pqueue_t *q, void **val);
int pqueue_get(pqueue_t *q, void **val);
void pqueue_close(pqueue_t *q);

#endif
