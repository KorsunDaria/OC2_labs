#ifndef PROXY_HTTP_H
#define PROXY_HTTP_H
#include <stddef.h>

typedef struct
{
	char method[8];
	int is_get, has_body_method;
	char *host;
	char port[8];
	char *path;
	char *key;
	size_t cl;
	char *ctype;
} request_t;

typedef struct
{
	int status;
	char *head;
	size_t head_len;
	int has_cl;
	unsigned long long cl;
	char *mime;
} response_t;

void request_free(request_t *r);
void response_free(response_t *r);

int http_parse_request(const char *buf, size_t len, request_t *r, const char **why);

char *http_build_upstream(const request_t *r, size_t *len);

int http_parse_response(const char *buf, size_t len, response_t *r, const char **why);

const char *http_reason(int status);
size_t http_build_error(char *dst, size_t cap, int status, const char *msg);

#endif
