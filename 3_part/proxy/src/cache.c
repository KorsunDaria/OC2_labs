#include "common.h"
#include "cache.h"
#include "log.h"
#include <sys/eventfd.h>
#include <errno.h>

#define NBUCKETS 1024
#define BLOCK (64 * 1024)
#define PASS_WINDOW (1024 * 1024)
#define COMPACT_MIN (32 * 1024)

struct cache
{
	pthread_mutex_t mu;
	pthread_cond_t mgr_cv;
	pthread_cond_t space_cv;
	entry_t *buckets[NBUCKETS];
	entry_t *lru_head, *lru_tail;
	size_t used, cap, max;
	int ttl_ms;
	int need_flag;
	size_t need;
	unsigned epoch;
	int last_fail;
	int stop, mgr_started;
	pthread_t mgr;
};

size_t cache_max(const cache_t *c) { return c->max; }

static unsigned hash_key(const char *s)
{
	unsigned h = 2166136261u; // из инета говорят норм начальное значение
	for (; *s; s++)
	{
		h ^= (unsigned char)*s;
		h *= 16777619u;
	}
	return h % NBUCKETS;
}

static void lru_unlink(cache_t *c, entry_t *e)
{
	if (e->lru_prev)
		e->lru_prev->lru_next = e->lru_next;
	else
		c->lru_head = e->lru_next;
	if (e->lru_next)
		e->lru_next->lru_prev = e->lru_prev;
	else
		c->lru_tail = e->lru_prev;
	e->lru_prev = e->lru_next = NULL;
}

static void lru_push_front(cache_t *c, entry_t *e)
{
	e->lru_prev = NULL;
	e->lru_next = c->lru_head;
	if (c->lru_head)
		c->lru_head->lru_prev = e;
	else
		c->lru_tail = e;
	c->lru_head = e;
}

static void map_remove(cache_t *c, entry_t *e)
{
	entry_t **pp = &c->buckets[hash_key(e->key)];
	while (*pp && *pp != e)
		pp = &(*pp)->hnext;
	if (*pp)
		*pp = e->hnext;
	e->hnext = NULL;
	lru_unlink(c, e);
	e->in_map = 0;
}

static void entry_free(cache_t *c, entry_t *e)
{
	c->used -= e->reserved;
	free(e->key);
	free(e->head);
	free(e->mime);
	free(e->data);
	CHK(pthread_cond_destroy(&e->drained));
	CHK(pthread_mutex_destroy(&e->mu));
	free(e);
}

static void drop_entry(cache_t *c, entry_t *e)
{
	if (e->in_map)
		map_remove(c, e);
	if (e->refs == 0)
		entry_free(c, e);
}

static int evict_one(cache_t *c)
{
	// идем с хвоста LRU
	for (entry_t *e = c->lru_tail; e; e = e->lru_prev)
	{
		if (e->refs == 0 && e->done)
		{
			LOGD(-1, e->key, "cache: evicted (LRU), %zu bytes", e->reserved);
			drop_entry(c, e);
			return 1;
		}
	}
	return 0;
}

static void sweep_expired(cache_t *c)
{
	int64_t now = now_ms();
	entry_t *nx;
	for (entry_t *e = c->lru_head; e; e = nx)
	{
		nx = e->lru_next;
		if (!e->done)
			continue;
		// TTL вышел
		if (now >= e->done_ms + c->ttl_ms)
		{
			LOGD(-1, e->key, "cache: TTL expired");
			drop_entry(c, e);
		}
	}
}

static void *mgr_main(void *arg)
{
	cache_t *c = arg;
	LOCK(&c->mu);
	while (!c->stop)
	{
		// просят место
		if (c->need_flag)
		{
			size_t need = c->need;
			c->need_flag = 0;
			c->need = 0;
			sweep_expired(c);
			int ok = 1;
			while (c->used + need > c->cap)
			{
				if (c->cap < c->max)
				{
					// удваиваем квоту
					size_t ncap = c->cap * 2;
					if (ncap > c->max || ncap < c->cap)
						ncap = c->max;
					c->cap = ncap;
					LOGN(-1, NULL, "cache: new segment, capacity now %zu bytes", c->cap);
				}
				else if (!evict_one(c))
				{
					ok = 0;
					break;
				}
			}
			c->last_fail = !ok;
			c->epoch++;
			// будим fetcherов
			CHK(pthread_cond_broadcast(&c->space_cv));
			continue;
		}
		CHK(pthread_cond_wait(&c->mgr_cv, &c->mu));
	}
	UNLOCK(&c->mu);
	return NULL;
}

cache_t *cache_create(size_t init_size, size_t max_size, int ttl_ms)
{
	cache_t *c = calloc(1, sizeof(*c));
	if (!c)
		return NULL;
	c->max = max_size;
	c->cap = init_size < max_size ? init_size : max_size;
	c->ttl_ms = ttl_ms;
	if (pthread_mutex_init(&c->mu, NULL) != 0)
		goto e1;
	if (pthread_cond_init(&c->mgr_cv, NULL) != 0)
		goto e2;
	if (pthread_cond_init(&c->space_cv, NULL) != 0)
		goto e3;
	return c;
e3:
	pthread_cond_destroy(&c->mgr_cv);
e2:
	pthread_mutex_destroy(&c->mu);
e1:
	free(c);
	return NULL;
}

int cache_start(cache_t *c)
{
	int rc = pthread_create(&c->mgr, NULL, mgr_main, c);
	if (rc)
	{
		LOGE(-1, NULL, "cache: cannot start manager thread: %s", strerror(rc));
		return -1;
	}
	c->mgr_started = 1;
	return 0;
}

void cache_stop(cache_t *c)
{
	LOCK(&c->mu);
	// стоп manager
	c->stop = 1;
	CHK(pthread_cond_signal(&c->mgr_cv));
	CHK(pthread_cond_broadcast(&c->space_cv));
	UNLOCK(&c->mu);
	if (c->mgr_started)
	{
		CHK(pthread_join(c->mgr, NULL));
		c->mgr_started = 0;
	}
}

void cache_destroy(cache_t *c)
{
	if (!c)
		return;
	LOCK(&c->mu);
	// чистим всё
	while (c->lru_head)
	{
		entry_t *e = c->lru_head;
		if (e->refs != 0)
			LOGE(-1, e->key, "cache: entry still referenced (%d) at destroy", e->refs);
		map_remove(c, e);
		entry_free(c, e);
	}
	UNLOCK(&c->mu);
	CHK(pthread_cond_destroy(&c->space_cv));
	CHK(pthread_cond_destroy(&c->mgr_cv));
	CHK(pthread_mutex_destroy(&c->mu));
	free(c);
}

static entry_t *entry_alloc(const char *key)
{
	entry_t *e = calloc(1, sizeof(*e));
	if (!e)
		return NULL;
	e->key = strdup(key);
	if (!e->key)
	{
		free(e);
		return NULL;
	}
	if (pthread_mutex_init(&e->mu, NULL) != 0)
	{
		free(e->key);
		free(e);
		return NULL;
	}
	if (pthread_cond_init(&e->drained, NULL) != 0)
	{
		pthread_mutex_destroy(&e->mu);
		free(e->key);
		free(e);
		return NULL;
	}
	e->state = E_INIT;
	return e;
}

static void reader_attach(entry_t *e, reader_t *rd)
{
	rd->off = 0;
	rd->prev = NULL;
	rd->next = e->readers;
	if (e->readers)
		e->readers->prev = rd;
	e->readers = rd;
	e->nreaders++;
}

entry_t *cache_acquire(cache_t *c, const char *key, int use_map, reader_t *rd, int *created)
{
	*created = 0;
	LOCK(&c->mu);
	entry_t *e = NULL;
	// ищем запись в карте
	if (use_map)
	{
		for (e = c->buckets[hash_key(key)]; e; e = e->hnext)
			if (strcmp(e->key, key) == 0)
				break;
		if (e && e->done && now_ms() >= e->done_ms + c->ttl_ms)
		{
			LOGD(-1, key, "cache: entry invalidated by TTL, refetching");
			drop_entry(c, e);
			e = NULL;
		}
	}
	if (e)
	{
		e->refs++;
		lru_unlink(c, e);
		lru_push_front(c, e);
	}
	else
	{
		// не нашли - новая
		e = entry_alloc(key);
		if (!e)
		{
			UNLOCK(&c->mu);
			return NULL;
		}
		e->refs = 2;
		*created = 1;
		if (use_map)
		{
			unsigned h = hash_key(key);
			e->hnext = c->buckets[h];
			c->buckets[h] = e;
			lru_push_front(c, e);
			e->in_map = 1;
		}
	}
	LOCK(&e->mu);
	reader_attach(e, rd);
	UNLOCK(&e->mu);
	UNLOCK(&c->mu);
	return e;
}

void entry_release(cache_t *c, entry_t *e, reader_t *rd)
{
	if (rd)
	{
		LOCK(&e->mu);
		if (rd->prev)
			rd->prev->next = rd->next;
		else
			e->readers = rd->next;
		if (rd->next)
			rd->next->prev = rd->prev;
		rd->prev = rd->next = NULL;
		e->nreaders--;
		CHK(pthread_cond_broadcast(&e->drained));
		UNLOCK(&e->mu);
	}
	LOCK(&c->mu);
	// последний ушел - чистим
	e->refs--;
	if (e->refs == 0 && !e->in_map)
		entry_free(c, e);
	UNLOCK(&c->mu);
}

static void notify_locked(entry_t *e)
{ //	будим подписчиков
	for (int i = 0; i < e->nsubs; i++)
	{
		if (eventfd_write(e->subs[i], 1) != 0 && errno != EAGAIN)
			LOGW(e->subs[i], e->key, "eventfd_write failed: %s", strerror(errno));
	}
	e->nsubs = 0;
}

static void subscribe_locked(entry_t *e, int efd)
{
	for (int i = 0; i < e->nsubs; i++)
		if (e->subs[i] == efd)
			return;
	if (e->nsubs < MAX_SUBS)
		e->subs[e->nsubs++] = efd;
}

void entry_set_headers(entry_t *e, char *head, size_t hl, char *mime)
{
	LOCK(&e->mu);
	e->head = head;
	e->head_len = hl;
	e->mime = mime;
	e->state = E_BODY;
	notify_locked(e);
	UNLOCK(&e->mu);
}

static int cache_reserve(cache_t *c, entry_t *e, size_t n)
{
	int ok = 0;
	LOCK(&c->mu);
	for (;;)
	{
		// место есть - резервируем
		if (c->used + n <= c->cap)
		{
			c->used += n;
			e->reserved += n;
			ok = 1;
			break;
		}
		if (c->stop || n > c->max)
			break;
		unsigned ep = c->epoch;
		c->need_flag = 1;
		if (n > c->need)
			c->need = n;
		CHK(pthread_cond_signal(&c->mgr_cv));
		while (c->epoch == ep && !c->stop)
			CHK(pthread_cond_wait(&c->space_cv, &c->mu));
		if (c->used + n <= c->cap)
			continue;
		if (c->last_fail || c->stop)
			break;
	}
	UNLOCK(&c->mu);
	return ok;
}

void entry_make_pass(cache_t *c, entry_t *e)
{
	LOCK(&c->mu);
	// убираем из карты - pass
	if (e->in_map)
		map_remove(c, e);
	c->used -= e->reserved;
	e->reserved = 0;
	UNLOCK(&c->mu);
	LOCK(&e->mu);
	e->pass = 1;
	UNLOCK(&e->mu);
}

int entry_prepare_known(cache_t *c, entry_t *e, size_t total)
{
	if (total > c->max || !cache_reserve(c, e, total ? total : 1))
		return 0;
	char *d = malloc(total ? total : 1);
	if (!d)
		return 0;
	LOCK(&e->mu);
	e->data = d;
	e->cap = total;
	UNLOCK(&e->mu);
	return 1;
}

static void compact_locked(entry_t *e)
{
	// самый медленный читатель
	size_t min = e->len;
	for (reader_t *r = e->readers; r; r = r->next)
		if (r->off < min)
			min = r->off;
	if (min > e->base && (min - e->base >= COMPACT_MIN || min == e->len))
	{
		size_t drop = min - e->base;
		memmove(e->data, e->data + drop, e->len - min);
		e->base = min;
	}
}

int entry_append(cache_t *c, entry_t *e, const char *buf, size_t n)
{
	if (!e->pass)
	{
		// хватит квоты?
		size_t need = e->len + n;
		if (need > e->reserved)
		{
			if (need > c->max)
			{
				LOGN(-1, e->key, "resource exceeds cache max size, streaming without caching");
				entry_make_pass(c, e);
			}
			else
			{
				size_t grow = ((need - e->reserved + BLOCK - 1) / BLOCK) * BLOCK;
				if (e->reserved + grow > c->max)
					grow = c->max - e->reserved;
				if (!cache_reserve(c, e, grow))
				{
					LOGN(-1, e->key, "no room in cache, streaming without caching");
					entry_make_pass(c, e);
				}
			}
		}
	}
	LOCK(&e->mu);
	if (e->pass)
	{
		// окно полное - ждем
		while (e->nreaders > 0 && e->len - e->base >= PASS_WINDOW)
			CHK(pthread_cond_wait(&e->drained, &e->mu));
		if (e->nreaders == 0)
		{
			UNLOCK(&e->mu);
			return 1;
		}
	}
	size_t stored = e->len - e->base;
	if (stored + n > e->cap)
	{
		size_t ncap = e->cap * 2;
		if (ncap < stored + n)
			ncap = stored + n;
		if (ncap < 4096)
			ncap = 4096;
		if (!e->pass && ncap > e->reserved)
			ncap = e->reserved;
		char *nd = realloc(e->data, ncap);
		if (!nd)
		{
			UNLOCK(&e->mu);
			return -1;
		}
		e->data = nd;
		e->cap = ncap;
	}
	memcpy(e->data + stored, buf, n);
	e->len += n;
	notify_locked(e);
	UNLOCK(&e->mu);
	return 0;
}

void entry_finish(cache_t *c, entry_t *e)
{
	LOCK(&c->mu);
	LOCK(&e->mu);
	// готово, TTL пошел
	e->state = E_DONE;
	if (e->in_map)
	{
		e->done = 1;
		e->done_ms = now_ms();
	}
	notify_locked(e);
	UNLOCK(&e->mu);
	UNLOCK(&c->mu);
}

void entry_fail(cache_t *c, entry_t *e, int status, const char *msg)
{
	LOCK(&c->mu);
	if (e->in_map)
		map_remove(c, e);
	UNLOCK(&c->mu);
	LOCK(&e->mu);
	e->state = E_FAILED;
	e->err_status = status;
	e->err_msg = msg;
	notify_locked(e);
	UNLOCK(&e->mu);
}

hres_t entry_get_head(entry_t *e, int efd, const char **head, size_t *hl,
					  int *err_status, const char **err_msg)
{
	hres_t r;
	LOCK(&e->mu);
	if (e->head)
	{
		*head = e->head;
		*hl = e->head_len;
		r = H_READY;
	}
	else if (e->state == E_FAILED)
	{
		*err_status = e->err_status;
		*err_msg = e->err_msg;
		r = H_ERROR;
	}
	else
	{
		subscribe_locked(e, efd);
		r = H_WAIT;
	}
	UNLOCK(&e->mu);
	return r;
}

rres_t entry_pull(entry_t *e, reader_t *rd, char *dst, size_t max, size_t *n, int efd)
{
	rres_t r;
	LOCK(&e->mu);
	// сколько осталось отдать
	size_t avail = e->len - rd->off;
	if (avail > 0)
	{
		size_t k = avail < max ? avail : max;
		memcpy(dst, e->data + (rd->off - e->base), k);
		rd->off += k;
		*n = k;
		if (e->pass)
		{
			compact_locked(e);
			CHK(pthread_cond_broadcast(&e->drained));
		}
		r = R_DATA;
	}
	else if (e->state == E_DONE)
	{
		r = R_EOF;
	}
	else if (e->state == E_FAILED)
	{
		r = R_ABORT;
	}
	else
	{
		subscribe_locked(e, efd);
		r = R_WAIT;
	}
	UNLOCK(&e->mu);
	return r;
}
