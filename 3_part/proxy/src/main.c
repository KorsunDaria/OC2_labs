#include "common.h"
#include "proxy.h"
#include "http.h"
#include "log.h"
#include <getopt.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <errno.h>

static void usage(const char *prog)
{
	printf("Usage: %s [options]\n\n"
		   "  -p, --port N                port to listen on (default 8080)\n"
		   "  -t, --max-client-threads N  client worker threads, 1..64 (default 4)\n"
		   "  -b, --backlog N             size of the queue of pending connections (default 128);\n"
		   "                              when it is full the client gets 503\n"
		   "  -i, --cache-initial-size S  initial cache size, suffixes K/M/G (default 1M)\n"
		   "  -m, --cache-max-size S      maximum cache size (default 10M)\n"
		   "  -l, --cache-ttl SEC         cache entry lifetime in seconds (default 5)\n"
		   "  -h, --help                  show this help\n",
		   prog);
}

static int parse_size(const char *s, size_t *out)
{
	char *end;
	errno = 0;
	unsigned long long v = strtoull(s, &end, 10);
	if (errno || end == s || s[0] == '-')
		return -1;
	unsigned long long mul = 1;
	if (*end)
	{
		switch (*end)
		{
		case 'k':
		case 'K':
			mul = 1ULL << 10;
			break;
		case 'm':
		case 'M':
			mul = 1ULL << 20;
			break;
		case 'g':
		case 'G':
			mul = 1ULL << 30;
			break;
		default:
			return -1;
		}
		if (end[1])
			return -1;
	}
	if (v > SIZE_MAX / mul)
		return -1;
	*out = (size_t)(v * mul);
	return 0;
}

static int parse_int(const char *s, int lo, int hi, int *out)
{
	char *end;
	errno = 0;
	long v = strtol(s, &end, 10);
	if (errno || end == s || *end || v < lo || v > hi)
		return -1;
	*out = (int)v;
	return 0;
}

static void reply_503(int fd, const char *msg)
{
	char buf[512];
	size_t n = http_build_error(buf, sizeof(buf), 503, msg);
	if (send(fd, buf, n, MSG_NOSIGNAL | MSG_DONTWAIT) < 0)
		LOGW(fd, NULL, "cannot send 503: %s", strerror(errno));
	close(fd);
}

int main(int argc, char **argv)
{
	proxy_t p;
	memset(&p, 0, sizeof(p));
	p.cfg = (config_t){.port = 8080, .client_threads = 4, .backlog = 128, .cache_init = 1u << 20, .cache_max = 10u << 20, .ttl_ms = 5000};

	static const struct option lo[] = {
		{"port", required_argument, 0, 'p'},
		{"max-client-threads", required_argument, 0, 't'},
		{"backlog", required_argument, 0, 'b'},
		{"cache-initial-size", required_argument, 0, 'i'},
		{"cache-max-size", required_argument, 0, 'm'},
		{"cache-ttl", required_argument, 0, 'l'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}};
	int o, bad = 0;
	// разбираем аргументы
	while ((o = getopt_long(argc, argv, "p:t:b:i:m:l:h", lo, NULL)) != -1)
	{
		int sec;
		switch (o)
		{
		case 'p':
			bad |= parse_int(optarg, 1, 65535, &p.cfg.port);
			break;
		case 't':
			bad |= parse_int(optarg, 1, MAX_SUBS, &p.cfg.client_threads);
			break;
		case 'b':
			bad |= parse_int(optarg, 1, 1000000, &p.cfg.backlog);
			break;
		case 'i':
			bad |= parse_size(optarg, &p.cfg.cache_init);
			break;
		case 'm':
			bad |= parse_size(optarg, &p.cfg.cache_max);
			break;
		case 'l':
			bad |= parse_int(optarg, 0, 86400, &sec);
			if (!bad)
				p.cfg.ttl_ms = sec * 1000;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			bad = 1;
		}
		if (bad)
		{
			fprintf(stderr, "invalid argument for option '%c'\n\n", o);
			usage(argv[0]);
			return 2;
		}
	}
	if (p.cfg.cache_max == 0 || p.cfg.cache_init == 0)
	{
		fprintf(stderr, "cache sizes must be positive\n");
		return 2;
	}

	sigset_t ss;
	sigemptyset(&ss);
	sigaddset(&ss, SIGINT);
	sigaddset(&ss, SIGTERM);
	int rc = pthread_sigmask(SIG_BLOCK, &ss, NULL);
	if (rc)
	{
		fprintf(stderr, "pthread_sigmask: %s\n", strerror(rc));
		return 1;
	}
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		perror("signal(SIGPIPE)");
		return 1;
	}
	// сигналы через fd
	int sfd = signalfd(-1, &ss, SFD_NONBLOCK | SFD_CLOEXEC);
	if (sfd < 0)
	{
		perror("signalfd");
		return 1;
	}

	int status = 1;
	int lfd = -1, spare = -1;
	// fd для остановки
	p.stop_fd = eventfd(0, EFD_CLOEXEC);
	if (p.stop_fd < 0)
	{
		perror("eventfd");
		close(sfd);
		return 1;
	}

	// создаем кэш и очереди
	p.cache = cache_create(p.cfg.cache_init, p.cfg.cache_max, p.cfg.ttl_ms);
	p.jobs = pqueue_create(65536);
	p.workers = calloc((size_t)p.cfg.client_threads, sizeof(*p.workers));
	p.fetchers = calloc(SERVER_THREADS, sizeof(*p.fetchers));
	if (!p.cache || !p.jobs || !p.workers || !p.fetchers)
	{
		LOGE(-1, NULL, "cannot allocate proxy structures");
		goto cleanup;
	}
	if (cache_start(p.cache) != 0)
		goto cleanup;

	int nw = 0;
	// запускаем клиентский пулл
	for (; nw < p.cfg.client_threads; nw++)
	{
		p.workers[nw] = worker_create(&p, nw);
		if (!p.workers[nw] || worker_start(p.workers[nw]) != 0)
			break;
	}
	if (nw != p.cfg.client_threads)
	{
		LOGE(-1, NULL, "cannot start client threads");
		goto shutdown;
	}
	// запускаем серверный пулл
	for (; p.nfetchers_started < SERVER_THREADS; p.nfetchers_started++)
	{
		rc = pthread_create(&p.fetchers[p.nfetchers_started], NULL, fetcher_main, &p);
		if (rc)
		{
			LOGE(-1, NULL, "cannot start server thread: %s", strerror(rc));
			goto shutdown;
		}
	}

	// слушающий сокет
	lfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (lfd < 0)
	{
		LOGE(-1, NULL, "socket(): %s", strerror(errno));
		goto shutdown;
	}
	int one = 1;
	if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0)
	{
		LOGE(lfd, NULL, "setsockopt(SO_REUSEADDR): %s", strerror(errno));
		goto shutdown;
	}
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons((uint16_t)p.cfg.port);
	if (bind(lfd, (struct sockaddr *)&sa, sizeof(sa)) != 0)
	{
		LOGE(lfd, NULL, "bind(port %d): %s", p.cfg.port, strerror(errno));
		goto shutdown;
	}
	if (listen(lfd, SOMAXCONN) != 0)
	{
		LOGE(lfd, NULL, "listen(): %s", strerror(errno));
		goto shutdown;
	}
	spare = open("/dev/null", O_RDONLY | O_CLOEXEC);
	if (spare < 0)
		LOGW(-1, NULL, "cannot reserve a spare descriptor: %s", strerror(errno));

	LOGN(-1, NULL, "Proxy listening on port %d: %d client threads, %d server threads, backlog %d, "
				   "cache %zu..%zu bytes, ttl %d ms",
		 p.cfg.port, p.cfg.client_threads, SERVER_THREADS,
		 p.cfg.backlog, p.cfg.cache_init, p.cfg.cache_max, p.cfg.ttl_ms);
	status = 0;

	// ждем клиента или сигнал
	struct pollfd pf[2] = {{lfd, POLLIN, 0}, {sfd, POLLIN, 0}};
	unsigned next_worker = 0;
	for (;;)
	{
		if (poll(pf, 2, -1) < 0)
		{
			if (errno == EINTR)
				continue;
			LOGE(-1, NULL, "poll(): %s", strerror(errno));
			status = 1;
			break;
		}
		if (pf[1].revents)
			break;
		if (!(pf[0].revents & POLLIN))
			continue;
		int fd = accept4(lfd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
		if (fd < 0)
		{
			if (errno == EMFILE || errno == ENFILE)
			{
				LOGW(lfd, NULL, "accept(): %s - out of descriptors", strerror(errno));
				if (spare >= 0)
				{
					close(spare);
					int t = accept4(lfd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
					if (t >= 0)
						reply_503(t, "proxy is out of file descriptors");
					spare = open("/dev/null", O_RDONLY | O_CLOEXEC);
				}
			}
			else if (errno != EAGAIN && errno != EINTR && errno != ECONNABORTED)
			{
				LOGW(lfd, NULL, "accept(): %s", strerror(errno));
			}
			continue;
		}
		worker_t *best = p.workers[next_worker++ % (unsigned)p.cfg.client_threads]; // round robin
		// отдаем воркеру, иначе 503
		int r = worker_submit(best, fd);
		if (r != 1)
		{
			LOGW(fd, NULL, r == 0 ? "pending queue is full, 503" : "cannot hand the connection over, 503");
			reply_503(fd, r == 0 ? "the proxy is overloaded, try again later" : "internal error");
		}
	}

shutdown:
	LOGN(-1, NULL, "Received shutdown signal");
	if (lfd >= 0)
	{
		close(lfd);
		lfd = -1;
		LOGN(-1, NULL, "Stopped accepting new connections");
	}
	// будим воркеров на стоп
	if (eventfd_write(p.stop_fd, 1) != 0)
		LOGE(-1, NULL, "eventfd_write(stop): %s", strerror(errno));
	for (int i = 0; i < p.cfg.client_threads; i++)
		if (p.workers[i])
			worker_join(p.workers[i]);
	LOGN(-1, NULL, "All connections closed");
	// закрываем очередь, ждем fetcherов
	if (p.jobs)
		pqueue_close(p.jobs);
	for (int i = 0; i < p.nfetchers_started; i++)
		CHK(pthread_join(p.fetchers[i], NULL));
	if (p.cache)
		cache_stop(p.cache);
	LOGN(-1, NULL, "All threads have finished");

cleanup:
	if (p.jobs)
	{
		void *v;
		// чистим остатки заданий
		while (pqueue_try_get(p.jobs, &v))
			job_destroy(&p, v, 1);
		pqueue_destroy(p.jobs);
	}
	if (p.workers)
	{
		for (int i = 0; i < p.cfg.client_threads; i++)
			worker_destroy(p.workers[i]);
		free(p.workers);
	}
	free(p.fetchers);
	cache_destroy(p.cache);
	if (spare >= 0)
		close(spare);
	if (lfd >= 0)
		close(lfd);
	close(p.stop_fd);
	close(sfd);
	LOGN(-1, NULL, "Proxy finished");
	return status;
}
