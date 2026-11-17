#ifndef PROXY_LOG_H
#define PROXY_LOG_H

typedef enum
{
	LVL_ERROR = 0,
	LVL_WARN,
	LVL_NOTICE,
	LVL_LOG,
	LVL_DEBUG
} log_level_t;

void log_write(log_level_t l, int fd, const char *url, const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));

#define LOGE(fd, url, ...) log_write(LVL_ERROR, (fd), (url), __VA_ARGS__)
#define LOGW(fd, url, ...) log_write(LVL_WARN, (fd), (url), __VA_ARGS__)
#define LOGN(fd, url, ...) log_write(LVL_NOTICE, (fd), (url), __VA_ARGS__)
#define LOGL(fd, url, ...) log_write(LVL_LOG, (fd), (url), __VA_ARGS__)
#define LOGD(fd, url, ...) log_write(LVL_DEBUG, (fd), (url), __VA_ARGS__)

#endif
