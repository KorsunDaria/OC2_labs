#include "common.h"
#include "proxy.h"
#include "http.h"
#include "log.h"
#include <poll.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <unistd.h>

#define OUT_CAP 32768
#define MAX_BODY (8u * 1024 * 1024)

// состояния сессии
typedef enum
{
	S_READ_REQ,
	S_READ_BODY,
	S_WAIT_HEAD,
	S_STREAM,
	S_ERROR_OUT
} sstate_t;

typedef struct session
{
	struct session *prev, *next;
	int fd;
	sstate_t st;
	char *in;
	size_t in_len, hdr_len;
	request_t req;
	int req_ok;
	char *body;
	size_t body_got;
	entry_t *e;
	reader_t rd;
	char out[OUT_CAP];
	size_t out_len, out_off;
	int waiting, ready, close_after_out;
} session_t;

// воркер ведет много сессий
struct worker
{
	proxy_t *p;
	int id;
	pthread_t tid;
	int started;
	int efd;
	pqueue_t *newq;
	session_t *head;
};

worker_t *worker_create(proxy_t *p, int id)
{
	worker_t *w = calloc(1, sizeof(*w));
	if (!w)
		return NULL;
	w->p = p;
	w->id = id;
	w->efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (w->efd < 0)
	{
		LOGE(-1, NULL, "eventfd(): %s", strerror(errno));
		free(w);
		return NULL;
	}
	// очередь новых сокетов
	w->newq = pqueue_create(p->cfg.backlog);
	if (!w->newq)
	{
		close(w->efd);
		free(w);
		return NULL;
	}
	return w;
}

int worker_submit(worker_t *w, int fd)
{
	// fd+1, чтобы не было NULL
	int rc = pqueue_try_add(w->newq, (void *)(intptr_t)(fd + 1));
	if (rc != 1)
		return rc;
	if (eventfd_write(w->efd, 1) != 0 && errno != EAGAIN)
	{
		LOGE(fd, NULL, "eventfd_write: %s", strerror(errno));
		return -1;
	}
	return 1;
}

static void s_unlink(worker_t *w, session_t *s)
{
	if (s->prev)
		s->prev->next = s->next;
	else
		w->head = s->next;
	if (s->next)
		s->next->prev = s->prev;
}

static void s_close(worker_t *w, session_t *s, const char *why)
{
	LOGD(s->fd, s->req_ok ? s->req.key : NULL, "session closed: %s", why);
	// отцепляемся от записи
	if (s->e)
		entry_release(w->p->cache, s->e, &s->rd);
	char tmp[512];
	for (int i = 0; i < 8; i++)
		if (recv(s->fd, tmp, sizeof(tmp), MSG_DONTWAIT) <= 0)
			break;
	close(s->fd);
	s_unlink(w, s);
	free(s->in);
	free(s->body);
	request_free(&s->req);
	free(s);
}

static void s_error(session_t *s, int status, const char *msg)
{
	LOGW(s->fd, s->req_ok ? s->req.key : NULL, "%d %s: %s", status, http_reason(status), msg);
	s->out_off = 0;
	s->out_len = http_build_error(s->out, OUT_CAP, status, msg);
	s->st = S_ERROR_OUT;
	s->close_after_out = 1;
	s->waiting = 0;
}

typedef enum
{
	F_DONE,
	F_BLOCKED,
	F_FAIL
} fres_t;

static fres_t s_flush(session_t *s)
{
	// досылаем буфер клиенту
	while (s->out_off < s->out_len)
	{
		ssize_t n = send(s->fd, s->out + s->out_off, s->out_len - s->out_off, MSG_NOSIGNAL);
		if (n > 0)
		{
			s->out_off += (size_t)n;
			continue;
		}
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return F_BLOCKED;
		LOGW(s->fd, s->req_ok ? s->req.key : NULL, "client write failed (%s), session dropped",
			 n < 0 ? strerror(errno) : "zero write");
		return F_FAIL;
	}
	s->out_off = s->out_len = 0;
	return F_DONE;
}

static void s_advance(worker_t *w, session_t *s)
{
	s->ready = 0;
	for (int iter = 0; iter < 4; iter++)
	{
		fres_t f = s_flush(s);
		if (f == F_FAIL)
		{
			s_close(w, s, "write error");
			return;
		}
		if (f == F_BLOCKED)
			return;
		if (s->close_after_out)
		{
			s_close(w, s, "finished");
			return;
		}

		// ждем заголовки из кэша
		if (s->st == S_WAIT_HEAD)
		{
			const char *head, *emsg;
			size_t hl;
			int es;
			hres_t h = entry_get_head(s->e, w->efd, &head, &hl, &es, &emsg);
			if (h == H_WAIT)
			{
				s->waiting = 1;
				return;
			}
			if (h == H_ERROR)
			{
				s_error(s, es, emsg);
				continue;
			}
			if (hl > OUT_CAP)
			{
				s_error(s, 502, "response head too large");
				continue;
			}
			memcpy(s->out, head, hl);
			s->out_len = hl;
			s->st = S_STREAM;
			continue;
		}
		// отдаем тело по кускам
		if (s->st == S_STREAM)
		{
			size_t n = 0;
			rres_t r = entry_pull(s->e, &s->rd, s->out, OUT_CAP, &n, w->efd);
			switch (r)
			{
			case R_DATA:
				s->out_len = n;
				s->out_off = 0;
				continue;
			case R_WAIT:
				s->waiting = 1;
				return;
			case R_EOF:
				LOGL(s->fd, s->req.key, "response delivered");
				s_close(w, s, "done");
				return;
			case R_ABORT:
				LOGW(s->fd, s->req.key, "origin failed mid-transfer, closing client connection");
				s_close(w, s, "upstream aborted");
				return;
			}
		}
	}
	s->ready = 1;
}

static void s_dispatch(worker_t *w, session_t *s)
{
	proxy_t *p = w->p;
	request_t *r = &s->req;
	int created = 0;
	LOGL(s->fd, r->key, "%s request", r->method);

	// ищем или создаем запись
	entry_t *e = cache_acquire(p->cache, r->key, r->is_get, &s->rd, &created);
	if (!e)
	{
		s_error(s, 500, "out of memory or cannot create a synchronization primitive");
		return;
	}
	s->e = e;
	s->st = S_WAIT_HEAD;
	if (!created)
	{
		LOGD(s->fd, r->key, "joined existing cache entry");
		return;
	}
	// собираем задание fetcher
	job_t *j = calloc(1, sizeof(*j));
	if (j)
	{
		j->e = e;
		j->host = strdup(r->host);
		strcpy(j->port, r->port);
		j->req = http_build_upstream(r, &j->req_len);
		j->is_get = r->is_get;
		j->body = s->body;
		j->body_len = r->cl;
		if (!j->host || !j->req)
		{
			j->body = NULL;
			job_destroy(p, j, 0);
			j = NULL;
		}
		else
			s->body = NULL;
	}
	// кладем в очередь заданий
	int rc = j ? pqueue_try_add(p->jobs, j) : -1;
	if (rc != 1)
	{
		int st = rc == 0 ? 503 : 500;
		entry_fail(p->cache, e, st, rc == 0 ? "too many pending downloads" : "out of memory");
		if (j)
		{
			j->body = NULL;
			job_destroy(p, j, 0);
		}
		entry_release(p->cache, e, NULL);
	}
}

static void s_parse(worker_t *w, session_t *s)
{
	const char *why = "";
	// разбираем запрос
	int rc = http_parse_request(s->in, s->hdr_len, &s->req, &why);
	if (rc != 0)
	{
		s_error(s, rc, why);
		return;
	}
	s->req_ok = 1;
	if (s->req.cl > MAX_BODY)
	{
		s_error(s, 400, "request body is too large");
		return;
	}
	if (s->req.has_body_method && s->req.cl > 0)
	{
		// буфер под тело POST
		s->body = malloc(s->req.cl);
		if (!s->body)
		{
			s_error(s, 500, "out of memory");
			return;
		}
		size_t have = s->in_len - s->hdr_len;
		if (have > s->req.cl)
			have = s->req.cl;
		memcpy(s->body, s->in + s->hdr_len, have);
		s->body_got = have;
		if (have < s->req.cl)
		{
			s->st = S_READ_BODY;
			return;
		}
	}
	s_dispatch(w, s);
}

static void s_on_readable(worker_t *w, session_t *s)
{
	if (s->st == S_READ_REQ)
	{
		ssize_t n = recv(s->fd, s->in + s->in_len, HDR_MAX - s->in_len, 0);
		if (n < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
				return;
			LOGW(s->fd, NULL, "recv(): %s", strerror(errno));
			s_close(w, s, "read error");
			return;
		}
		if (n == 0)
		{
			s_close(w, s, "client closed before sending a request");
			return;
		}
		size_t old = s->in_len;
		s->in_len += (size_t)n;
		size_t from = old >= 3 ? old - 3 : 0;
		char *end = memmem(s->in + from, s->in_len - from, "\r\n\r\n", 4);
		if (!end)
		{
			if (s->in_len >= HDR_MAX)
				s_error(s, 400, "request headers exceed the 16 KiB limit");
			return;
		}
		s->hdr_len = (size_t)(end - s->in) + 4;
		s_parse(w, s);
	}
	else if (s->st == S_READ_BODY)
	{
		// дочитываем тело
		ssize_t n = recv(s->fd, s->body + s->body_got, s->req.cl - s->body_got, 0);
		if (n < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
				return;
			LOGW(s->fd, s->req.key, "recv(): %s", strerror(errno));
			s_close(w, s, "read error");
			return;
		}
		if (n == 0)
		{
			LOGW(s->fd, s->req.key, "client closed in the middle of the request body");
			s_close(w, s, "client reset");
			return;
		}
		s->body_got += (size_t)n;
		if (s->body_got < s->req.cl)
			return;
		s_dispatch(w, s);
	}
	if (s->st != S_READ_REQ && s->st != S_READ_BODY)
		s_advance(w, s);
}

static void adopt(worker_t *w, int fd)
{
	session_t *s = calloc(1, sizeof(*s));
	if (s)
		s->in = malloc(HDR_MAX);
	if (!s || !s->in)
	{
		char buf[256];
		size_t n = http_build_error(buf, sizeof(buf), 500, "out of memory");
		if (send(fd, buf, n, MSG_NOSIGNAL | MSG_DONTWAIT) < 0)
			LOGW(fd, NULL, "send(): %s", strerror(errno));
		LOGE(fd, NULL, "cannot allocate a session");
		close(fd);
		free(s);
		return;
	}
	s->fd = fd;
	s->st = S_READ_REQ;
	// добавляем в список сессий
	s->next = w->head;
	if (w->head)
		w->head->prev = s;
	w->head = s;
	LOGD(fd, NULL, "session adopted by worker %d", w->id);
}

static void *worker_main(void *arg)
{
	worker_t *w = arg;
	proxy_t *p = w->p;
	size_t cap = 0;
	struct pollfd *pf = NULL;
	session_t **ss = NULL;

	for (;;)
	{
		size_t n = 0;
		// считаем сессии
		for (session_t *s = w->head; s; s = s->next)
			n++;
		if (n + 2 > cap)
		{
			size_t nc = (n + 2) * 2;
			struct pollfd *npf = realloc(pf, nc * sizeof(*pf));
			if (npf)
				pf = npf;
			session_t **nss = realloc(ss, nc * sizeof(*ss));
			if (nss)
				ss = nss;
			if (!npf || !nss)
			{
				LOGE(-1, NULL, "worker %d: out of memory", w->id);
				break;
			}
			cap = nc;
		}
		// собираем массив для poll
		pf[0] = (struct pollfd){w->efd, POLLIN, 0};
		pf[1] = (struct pollfd){p->stop_fd, POLLIN, 0};
		size_t k = 2;
		int any_ready = 0;
		for (session_t *s = w->head; s; s = s->next)
		{
			short ev = 0;
			if (s->st == S_READ_REQ || s->st == S_READ_BODY)
			{
				ev = POLLIN;
			}
			else if (s->out_off < s->out_len)
			{
				ev = POLLOUT;
			}
			if (s->ready)
				any_ready = 1;
			pf[k] = (struct pollfd){s->fd, ev, 0};
			ss[k] = s;
			k++;
		}

		// ждем события
		int rc = poll(pf, (nfds_t)k, any_ready ? 0 : -1);
		if (rc < 0)
		{
			if (errno == EINTR)
				continue;
			LOGE(-1, NULL, "worker %d: poll(): %s", w->id, strerror(errno));
			break;
		}
		if (pf[1].revents)
			break;

		for (size_t i = 2; i < k; i++)
		{
			session_t *s = ss[i];
			short re = pf[i].revents;
			if (!re)
				continue;
			if (re & POLLIN)
			{
				s_on_readable(w, s);
				continue;
			}
			if (re & (POLLERR | POLLHUP | POLLNVAL))
			{
				LOGW(s->fd, s->req_ok ? s->req.key : NULL, "client connection reset");
				s_close(w, s, "reset");
				continue;
			}
			if (re & POLLOUT)
				s_advance(w, s);
		}

		// разбудил eventfd
		if (pf[0].revents)
		{
			eventfd_t cnt;
			if (eventfd_read(w->efd, &cnt) != 0 && errno != EAGAIN)
				LOGE(-1, NULL, "eventfd_read: %s", strerror(errno));
			void *v;
			// забираем новые сокеты
			while (pqueue_try_get(w->newq, &v))
				adopt(w, (int)(intptr_t)v - 1);
			session_t *nx;
			for (session_t *s = w->head; s; s = nx)
			{
				nx = s->next;
				if (s->waiting)
				{
					s->waiting = 0;
					s_advance(w, s);
				}
			}
		}

		session_t *nx;
		for (session_t *s = w->head; s; s = nx)
		{
			nx = s->next;
			if (s->ready)
				s_advance(w, s);
		}
	}

	while (w->head)
		s_close(w, w->head, "shutdown");
	void *v;
	while (pqueue_try_get(w->newq, &v))
	{
		close((int)(intptr_t)v - 1);
	}
	free(pf);
	free(ss);
	LOGD(-1, NULL, "worker %d finished", w->id);
	return NULL;
}

int worker_start(worker_t *w)
{
	int rc = pthread_create(&w->tid, NULL, worker_main, w);
	if (rc)
	{
		LOGE(-1, NULL, "cannot create worker thread: %s", strerror(rc));
		return -1;
	}
	w->started = 1;
	return 0;
}

void worker_join(worker_t *w)
{
	if (w->started)
	{
		CHK(pthread_join(w->tid, NULL));
		w->started = 0;
	}
}

void worker_destroy(worker_t *w)
{
	if (!w)
		return;
	pqueue_destroy(w->newq);
	close(w->efd);
	free(w);
}
