#include "common.h"
#include "proxy.h"
#include "http.h"
#include "log.h"
#include <poll.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

void job_destroy(proxy_t *p, job_t *j, int release_entry)
{
	if (!j)
		return;
	if (release_entry && j->e)
		entry_release(p->cache, j->e, NULL);
	free(j->host);
	free(j->req);
	free(j->body);
	free(j);
}

static int wait_fd(proxy_t *p, int fd, short ev, int timeout_ms)
{
	struct pollfd pf[2] = {{fd, ev, 0}, {p->stop_fd, POLLIN, 0}};
	int64_t end = now_ms() + timeout_ms;
	for (;;)
	{
		int64_t left = end - now_ms();
		if (left < 0)
			left = 0;
		int rc = poll(pf, 2, (int)left);
		if (rc < 0)
		{
			if (errno == EINTR)
				continue;
			return -2;
		}
		if (rc == 0)
			return 0;
		if (pf[1].revents)
			return -1;
		return 1;
	}
}

static void fail(proxy_t *p, job_t *j, int fd, int status, const char *msg)
{
	LOGW(fd, j->e->key, "%d %s: %s", status, http_reason(status), msg);
	entry_fail(p->cache, j->e, status, msg);
}

static int connect_origin(proxy_t *p, job_t *j, int *status, const char **msg)
{
	struct addrinfo hints, *res = NULL;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	// резолвим имя
	int rc = getaddrinfo(j->host, j->port, &hints, &res);
	if (rc != 0)
	{
		LOGW(-1, j->e->key, "getaddrinfo(%s): %s", j->host, gai_strerror(rc));
		if (rc == EAI_MEMORY || rc == EAI_SYSTEM)
		{
			*status = 500;
			*msg = "name resolution failed with a system error";
		}
		else
		{
			*status = 502;
			*msg = "cannot resolve the target host name";
		}
		return -1;
	}
	int fd = -1;
	*status = 502;
	*msg = "cannot connect to the target server";
	// пробуем все адреса
	for (struct addrinfo *ai = res; ai; ai = ai->ai_next)
	{
		fd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC, ai->ai_protocol);
		if (fd < 0)
		{
			LOGE(-1, j->e->key, "socket(): %s", strerror(errno));
			*status = 500;
			*msg = "cannot create a socket";
			continue;
		}
		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
			break;
		if (errno != EINPROGRESS)
		{
			LOGW(fd, j->e->key, "connect(): %s", strerror(errno));
			*status = 502;
			*msg = "connection to the target server failed";
			close(fd);
			fd = -1;
			continue;
		}
		int w = wait_fd(p, fd, POLLOUT, IO_TIMEOUT_MS);
		if (w == 0)
		{
			*status = 504;
			*msg = "timeout while connecting to the target server";
			close(fd);
			fd = -1;
			continue;
		}
		if (w < 0)
		{
			*status = 500;
			*msg = "proxy is shutting down";
			close(fd);
			fd = -1;
			break;
		}
		// проверяем итог connect
		int err = 0;
		socklen_t el = sizeof(err);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el) != 0)
			err = errno;
		if (err == 0)
			break;
		LOGW(fd, j->e->key, "connect(): %s", strerror(err));
		*status = 502;
		*msg = err == ECONNREFUSED ? "connection refused by the target server"
								   : "connection to the target server failed";
		close(fd);
		fd = -1;
	}
	freeaddrinfo(res);
	return fd;
}

static int send_all(proxy_t *p, int fd, const char *buf, size_t len, const char **msg)
{
	size_t off = 0;
	while (off < len)
	{
		ssize_t n = send(fd, buf + off, len - off, MSG_NOSIGNAL);
		if (n > 0)
		{
			off += (size_t)n;
			continue;
		}
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
		{
			*msg = "failed to send the request to the target server";
			return 502;
		}
		int w = wait_fd(p, fd, POLLOUT, IO_TIMEOUT_MS);
		if (w == 0)
		{
			*msg = "timeout - sending the request to the target server";
			return 504;
		}
		if (w < 0)
		{
			*msg = "proxy is shutting down";
			return 500;
		}
	}
	return 0;
}

static void fetch(proxy_t *p, job_t *j)
{
	entry_t *e = j->e;
	cache_t *c = p->cache;
	int status;
	const char *msg;
	int fd = connect_origin(p, j, &status, &msg);
	if (fd < 0)
	{
		fail(p, j, -1, status, msg);
		return;
	}
	LOGD(fd, e->key, "connected to origin");

	// шлем запрос и тело
	if ((status = send_all(p, fd, j->req, j->req_len, &msg)) != 0 ||
		(j->body_len && (status = send_all(p, fd, j->body, j->body_len, &msg)) != 0))
	{
		fail(p, j, fd, status, msg);
		goto out;
	}

	char hb[HDR_MAX];
	size_t hl = 0;
	char *hend = NULL;
	while (!hend)
	{
		if (hl == sizeof(hb))
		{
			fail(p, j, fd, 502, "response headers of the target server are too large");
			goto out;
		}
		// читаем заголовки
		ssize_t n = recv(fd, hb + hl, sizeof(hb) - hl, 0);
		if (n > 0)
		{
			size_t from = hl >= 3 ? hl - 3 : 0;
			hl += (size_t)n;
			hend = memmem(hb + from, hl - from, "\r\n\r\n", 4);
			continue;
		}
		if (n == 0)
		{
			fail(p, j, fd, 502, "target server closed the connection before sending a full response");
			goto out;
		}
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			fail(p, j, fd, 502, "connection to the target server was reset");
			goto out;
		}
		int w = wait_fd(p, fd, POLLIN, IO_TIMEOUT_MS);
		if (w == 0)
		{
			fail(p, j, fd, 504, "timeout - waiting for the target server response");
			goto out;
		}
		if (w < 0)
		{
			fail(p, j, fd, 500, "proxy is shutting down");
			goto out;
		}
	}
	size_t head_len = (size_t)(hend - hb) + 4;
	response_t rs;
	const char *why = "";
	// разбираем ответ
	int pr = http_parse_response(hb, head_len, &rs, &why);
	if (pr != 0)
	{
		fail(p, j, fd, pr == -2 ? 500 : 502, pr == -2 ? "out of memory" : "target server returned an invalid HTTP response");
		if (pr == -1)
			LOGW(fd, e->key, "invalid response: %s", why);
		goto out;
	}

	int no_body = rs.status == 204 || rs.status == 304 || rs.status < 200;

	int want_cache = (rs.status == 200 && j->is_get);
	// кэшируем только 200 GET
	if (want_cache)
	{
		if (rs.has_cl)
		{
			if (rs.cl > cache_max(c) || !entry_prepare_known(c, e, (size_t)rs.cl))
			{
				LOGN(fd, e->key, "%llu bytes do not fit the cache, streaming without caching", rs.cl);
				entry_make_pass(c, e);
			}
		}
	}
	else
	{
		entry_make_pass(c, e);
	}
	LOGL(fd, e->key, "origin answered %d%s%s", rs.status, rs.mime ? ", " : "", rs.mime ? rs.mime : "");
	entry_set_headers(e, rs.head, rs.head_len, rs.mime);
	rs.head = NULL;
	rs.mime = NULL;
	int has_cl = rs.has_cl;
	unsigned long long remaining = rs.cl;
	response_free(&rs);

	if (no_body)
	{
		entry_finish(c, e);
		goto out;
	}
	// качаем тело кусками
	char buf[65536];
	size_t have = hl - head_len;
	const char *src = hb + head_len;
	for (;;)
	{
		size_t n = have;
		if (has_cl && n > remaining)
			n = (size_t)remaining;
		if (n > 0)
		{
			// кладем кусок в кэш
			int ar = entry_append(c, e, src, n);
			if (ar == 1)
			{
				LOGL(fd, e->key, "all clients gone, download cancelled");
				entry_fail(c, e, 500, "cancelled");
				goto out;
			}
			if (ar < 0)
			{
				fail(p, j, fd, 500, "out of memory");
				goto out;
			}
			if (has_cl)
				remaining -= n;
		}
		if (has_cl && remaining == 0)
		{
			entry_finish(c, e);
			LOGL(fd, e->key, "download complete");
			goto out;
		}
		ssize_t r = recv(fd, buf, sizeof(buf), 0);
		if (r > 0)
		{
			src = buf;
			have = (size_t)r;
			continue;
		}
		if (r == 0)
		{
			if (has_cl)
			{
				fail(p, j, fd, 502, "target server closed the connection before the full body was received");
			}
			else
			{
				entry_finish(c, e);
				LOGL(fd, e->key, "download complete (connection closed by origin)");
			}
			goto out;
		}
		if (errno == EINTR)
		{
			have = 0;
			continue;
		}
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			fail(p, j, fd, 502, "connection to the target server was reset");
			goto out;
		}
		have = 0;
		int w = wait_fd(p, fd, POLLIN, IO_TIMEOUT_MS);
		if (w == 0)
		{
			fail(p, j, fd, 504, "timeout - reading from the target server");
			goto out;
		}
		if (w < 0)
		{
			fail(p, j, fd, 500, "proxy is shutting down");
			goto out;
		}
	}
out:
	close(fd);
}

void *fetcher_main(void *arg)
{
	proxy_t *p = arg;
	void *v;
	// берем задания из очереди
	while (pqueue_get(p->jobs, &v))
	{
		job_t *j = v;
		fetch(p, j);
		job_destroy(p, j, 1);
	}
	return NULL;
}
