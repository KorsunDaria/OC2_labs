#ifndef PROXY_PROXY_H
#define PROXY_PROXY_H
#include "cache.h"
#include "pqueue.h"

typedef struct
{
	int port;
	int client_threads;
	int backlog;
	size_t cache_init, cache_max;
	int ttl_ms;
} config_t;

typedef struct worker worker_t;

typedef struct proxy
{
	config_t cfg;
	int stop_fd;
	cache_t *cache;
	pqueue_t *jobs;
	worker_t **workers;
	pthread_t *fetchers;
	int nfetchers_started;
} proxy_t;

worker_t *worker_create(proxy_t *p, int id);
int worker_start(worker_t *w);
int worker_submit(worker_t *w, int fd);
void worker_join(worker_t *w);
void worker_destroy(worker_t *w);

typedef struct job
{
	entry_t *e;
	char *host, port[8];
	char *req;
	size_t req_len;
	char *body;
	size_t body_len;
	int is_get;
} job_t;

void job_destroy(proxy_t *p, job_t *j, int release_entry);
void *fetcher_main(void *arg);

#endif
