#ifndef PROXY_COMMON_H
#define PROXY_COMMON_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#define HDR_MAX 16384
#define IO_TIMEOUT_MS 10000
#define SERVER_THREADS 64

#define CHK(call)                                             \
	do                                                        \
	{                                                         \
		int e_ = (call);                                      \
		if (e_)                                               \
		{                                                     \
			fprintf(stderr, "%s:%d: %s failed: %s\n",         \
					__FILE__, __LINE__, #call, strerror(e_)); \
			abort();                                          \
		}                                                     \
	} while (0)

#define LOCK(m) CHK(pthread_mutex_lock(m))
#define UNLOCK(m) CHK(pthread_mutex_unlock(m))

static inline int64_t now_ms(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
	{
		perror("clock_gettime");
		abort();
	}
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#endif
