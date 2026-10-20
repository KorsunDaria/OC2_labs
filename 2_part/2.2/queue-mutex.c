#define _GNU_SOURCE
#include "queue.h"

#include <string.h>
#include <errno.h>

#define CHK(call) do { int e_ = (call); if (e_) { \
    fprintf(stderr, "%s:%d: %s failed: %s\n", \
            __FILE__, __LINE__, #call, strerror(e_)); \
    abort(); } } while (0)

queue_t* queue_init(int max_count) {
    queue_t *q = malloc(sizeof(queue_t));
    if (!q) {
        fprintf(stderr, "queue_init: cannot allocate memory: %s\n", strerror(errno));
        abort();
    }

    int e = pthread_mutex_init(&q->mutex, NULL);
    if (e) {
        fprintf(stderr, "queue_init: pthread_mutex_init failed: %s\n", strerror(e));
        free(q);                
        abort();
    }

    queue_common_init(q, max_count);
    return q;
}

void queue_destroy(queue_t *q) {
    if (!q) return;
    queue_common_destroy(q);
    CHK(pthread_mutex_destroy(&q->mutex));  
    free(q);
}

int queue_add(queue_t *q, int val) {
    CHK(pthread_mutex_lock(&q->mutex));
    q->add_attempts++;
    if (q->count == q->max_count) {
        CHK(pthread_mutex_unlock(&q->mutex));
        return 0;
    }
    qlist_push(q, val);
    CHK(pthread_mutex_unlock(&q->mutex));
    return 1;
}

int queue_get(queue_t *q, int *val) {
    CHK(pthread_mutex_lock(&q->mutex));
    q->get_attempts++;
    if (q->count == 0) {
        CHK(pthread_mutex_unlock(&q->mutex));
        return 0;
    }
    *val = qlist_pop(q);
    CHK(pthread_mutex_unlock(&q->mutex));
    return 1;
}