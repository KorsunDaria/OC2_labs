#include "common.h"
#include "log.h"
#include <stdarg.h>
#include <sys/syscall.h>
#include <unistd.h>

static pthread_mutex_t log_mu = PTHREAD_MUTEX_INITIALIZER;
#ifndef LOG_LEVEL
#define LOG_LEVEL LVL_LOG
#endif

static const char *names[] = {"ERROR", "WARN", "NOTICE", "LOG", "DEBUG"};

void log_write(log_level_t l, int fd, const char *url, const char *fmt, ...)
{
	if ((int)l > LOG_LEVEL)
		return;

	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	struct timespec ts;
	struct tm tm;
	char tbuf[16] = "??:??:??";
	long ms = 0;
	if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
	{
		time_t sec = ts.tv_sec;
		if (localtime_r(&sec, &tm))
			strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tm);
		ms = ts.tv_nsec / 1000000;
	}

	// лочим, чтобы строки не мешались
	LOCK(&log_mu);
	printf("%s.%03ld [%-6s] [t%ld]", tbuf, ms, names[l], (long)syscall(SYS_gettid));
	if (fd >= 0)
		printf(" [fd=%d]", fd);
	if (url)
		printf(" [%s]", url);
	printf(" %s\n", msg);
	fflush(stdout);
	UNLOCK(&log_mu);
}
