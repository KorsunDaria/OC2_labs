#ifndef PROXY_CACHE_H
#define PROXY_CACHE_H
#include <stddef.h>
#include <pthread.h>
#include <stdint.h>

#define MAX_SUBS 64

typedef enum { E_INIT, E_BODY, E_DONE, E_FAILED } estate_t;

typedef struct reader {
	size_t off;                          
	struct reader *prev, *next;
} reader_t;

typedef struct entry {
	char *key;                           
	int refs;
	int in_map, done;
	int64_t done_ms;
	size_t reserved;                     
	struct entry *hnext, *lru_prev, *lru_next;
	pthread_mutex_t mu;
	pthread_cond_t drained;
	estate_t state;
	int pass;                            
	char *head; size_t head_len; char *mime;
	char *data; size_t cap;
	size_t len;                          
	size_t base;                         
	int err_status; const char *err_msg;
	reader_t *readers; int nreaders;
	int subs[MAX_SUBS]; int nsubs;       
} entry_t;

typedef struct cache cache_t;

cache_t *cache_create(size_t init_size, size_t max_size, int ttl_ms);
int  cache_start(cache_t *c);            
void cache_stop(cache_t *c);             
void cache_destroy(cache_t *c);
size_t cache_max(const cache_t *c);

entry_t *cache_acquire(cache_t *c, const char *key, int use_map, reader_t *rd, int *created);
void entry_release(cache_t *c, entry_t *e, reader_t *rd);

void entry_set_headers(entry_t *e, char *head, size_t hl, char *mime);   
int  entry_prepare_known(cache_t *c, entry_t *e, size_t total);         
void entry_make_pass(cache_t *c, entry_t *e);
int  entry_append(cache_t *c, entry_t *e, const char *buf, size_t n);   
void entry_finish(cache_t *c, entry_t *e);
void entry_fail(cache_t *c, entry_t *e, int status, const char *msg);

typedef enum { H_READY, H_WAIT, H_ERROR } hres_t;
typedef enum { R_DATA, R_WAIT, R_EOF, R_ABORT } rres_t;
hres_t entry_get_head(entry_t *e, int efd, const char **head, size_t *hl,
		      int *err_status, const char **err_msg);
rres_t entry_pull(entry_t *e, reader_t *rd, char *dst, size_t max, size_t *n, int efd);

#endif
