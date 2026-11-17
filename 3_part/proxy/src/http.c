#include "common.h"
#include "http.h"
#include <ctype.h>
#include <strings.h>

const char *http_reason(int s)
{
	switch (s)
	{
	case 200:
		return "OK";
	case 400:
		return "Bad Request";
	case 500:
		return "Internal Server Error";
	case 501:
		return "Not Implemented";
	case 502:
		return "Bad Gateway";
	case 503:
		return "Service Unavailable";
	case 504:
		return "Gateway Timeout";
	default:
		return "Status";
	}
}

size_t http_build_error(char *dst, size_t cap, int status, const char *msg)
{
	char body[512];
	int bl = snprintf(body, sizeof(body), "%d %s: %s\n", status, http_reason(status), msg);
	if (bl < 0 || (size_t)bl >= sizeof(body))
		bl = (int)sizeof(body) - 1;
	int n = snprintf(dst, cap,
					 "HTTP/1.0 %d %s\r\nContent-Type: text/plain; charset=utf-8\r\n"
					 "Content-Length: %d\r\nConnection: close\r\n\r\n%.*s",
					 status, http_reason(status), bl, bl, body);
	if (n < 0 || (size_t)n >= cap)
		return 0;
	return (size_t)n;
}

void request_free(request_t *r)
{
	free(r->host);
	free(r->path);
	free(r->key);
	free(r->ctype);
	memset(r, 0, sizeof(*r));
}

void response_free(response_t *r)
{
	free(r->head);
	free(r->mime);
	memset(r, 0, sizeof(*r));
}

static int is_tchar(unsigned char c)
{
	return isalnum(c) || strchr("!#$%&'*+-.^_`|~", c) != NULL;
}

static const char *next_line(const char *p, const char *end, size_t *len)
{
	const char *nl = memchr(p, '\n', (size_t)(end - p));
	if (!nl)
		return NULL;
	size_t l = (size_t)(nl - p);
	if (l > 0 && p[l - 1] == '\r')
		l--;
	*len = l;
	return nl + 1;
}

static int has_ctl(const char *s, size_t n)
{
	for (size_t i = 0; i < n; i++)
	{
		unsigned char c = (unsigned char)s[i];
		if ((c < 0x20 && c != '\t') || c == 0x7f)
			return 1;
	}
	return 0;
}

static char *dupn(const char *s, size_t n)
{
	char *r = malloc(n + 1);
	if (!r)
		return NULL;
	memcpy(r, s, n);
	r[n] = 0;
	return r;
}

static int parse_authority(const char *a, size_t n, char **host, char port[8])
{
	const char *hs = a, *he, *ps = NULL;
	if (n == 0)
		return -1;
	if (a[0] == '[')
	{
		const char *rb = memchr(a, ']', n);
		if (!rb)
			return -1;
		hs = a + 1;
		he = rb;
		if (rb + 1 < a + n)
		{
			if (rb[1] != ':')
				return -1;
			ps = rb + 2;
		}
	}
	else
	{
		const char *c = memchr(a, ':', n);
		he = c ? c : a + n;
		if (c)
			ps = c + 1;
	}
	if (he == hs)
		return -1;
	for (const char *q = hs; q < he; q++)
		if (isspace((unsigned char)*q) || *q == '/' || *q == '@')
			return -1;
	strcpy(port, "80");
	if (ps)
	{
		size_t pl = (size_t)(a + n - ps);
		if (pl == 0 || pl > 5)
			return -1;
		long v = 0;
		for (size_t i = 0; i < pl; i++)
		{
			if (!isdigit((unsigned char)ps[i]))
				return -1;
			v = v * 10 + (ps[i] - '0');
		}
		if (v < 1 || v > 65535)
			return -1;
		memcpy(port, ps, pl);
		port[pl] = 0;
	}
	*host = dupn(hs, (size_t)(he - hs));
	return *host ? 0 : -2;
}

static int hdr_is(const char *name, size_t nl, const char *want)
{
	return nl == strlen(want) && strncasecmp(name, want, nl) == 0;
}

int http_parse_request(const char *buf, size_t len, request_t *r, const char **why)
{
	memset(r, 0, sizeof(*r));
	const char *end = buf + len, *p = buf;
	size_t ll;
	int rc = 400;
	char *hosthdr = NULL;
	int have_cl = 0;
	size_t cl = 0;

	// стартовая строка
	p = next_line(buf, end, &ll);
	if (!p)
	{
		*why = "malformed request line";
		return 400;
	}
	const char *line = buf;
	const char *sp1 = memchr(line, ' ', ll);
	if (!sp1)
	{
		*why = "malformed request line";
		return 400;
	}
	const char *sp2 = memchr(sp1 + 1, ' ', (size_t)(line + ll - sp1 - 1));
	if (!sp2)
	{
		*why = "malformed request line";
		return 400;
	}
	size_t ml = (size_t)(sp1 - line);
	if (ml == 0 || ml > 7)
	{
		*why = "malformed method";
		return 400;
	}
	for (size_t i = 0; i < ml; i++)
		if (!isupper((unsigned char)line[i]))
		{
			*why = "malformed method";
			return 400;
		}
	memcpy(r->method, line, ml);
	r->method[ml] = 0;
	if ((size_t)(line + ll - sp2 - 1) != 8 ||
		(memcmp(sp2 + 1, "HTTP/1.0", 8) != 0 && memcmp(sp2 + 1, "HTTP/1.1", 8) != 0))
	{
		*why = "unsupported or malformed HTTP version";
		return 400;
	}
	const char *uri = sp1 + 1;
	size_t ul = (size_t)(sp2 - uri);
	if (ul == 0 || has_ctl(uri, ul))
	{
		*why = "malformed request target";
		return 400;
	}

	r->is_get = strcmp(r->method, "GET") == 0;
	r->has_body_method = !strcmp(r->method, "POST") || !strcmp(r->method, "PUT");
	int known = r->is_get || r->has_body_method || !strcmp(r->method, "DELETE");

	char *auth_host = NULL;
	char auth_port[8] = "80";
	const char *path = NULL;
	size_t pl = 0;
	int absolute = 0;

	if (ul >= 7 && strncasecmp(uri, "http://", 7) == 0)
	{
		const char *a = uri + 7, *ae = uri + ul;
		const char *q = a;
		while (q < ae && *q != '/' && *q != '?')
			q++;
		int e = parse_authority(a, (size_t)(q - a), &auth_host, auth_port);
		if (e == -2)
		{
			*why = "out of memory";
			return 500;
		}
		if (e)
		{
			*why = "malformed URL authority";
			return 400;
		}
		absolute = 1;
		if (q == ae)
		{
			path = "/";
			pl = 1;
		}
		else if (*q == '?')
		{
			path = NULL;
		}
		else
		{
			path = q;
			pl = (size_t)(ae - q);
		}
		if (q < ae && *q == '?')
		{
			r->path = malloc((size_t)(ae - q) + 2);
			if (!r->path)
			{
				free(auth_host);
				*why = "out of memory";
				return 500;
			}
			r->path[0] = '/';
			memcpy(r->path + 1, q, (size_t)(ae - q));
			r->path[(size_t)(ae - q) + 1] = 0;
		}
	}
	else if (uri[0] == '/')
	{
		path = uri;
		pl = ul;
	}
	else
	{
		*why = known ? "only absolute http:// URLs are supported" : "method not supported";
		return known ? 400 : 501;
	}
	if (!r->path && path)
	{
		r->path = dupn(path, pl);
		if (!r->path)
		{
			free(auth_host);
			*why = "out of memory";
			return 500;
		}
	}

	for (;;)
	{
		const char *ls = p;
		p = next_line(p, end, &ll);
		if (!p)
		{
			rc = 400;
			*why = "unterminated headers";
			goto fail;
		}
		if (ll == 0)
			break;
		if (has_ctl(ls, ll))
		{
			rc = 400;
			*why = "control character in header";
			goto fail;
		}
		if (ls[0] == ' ' || ls[0] == '\t')
		{
			rc = 400;
			*why = "obsolete header folding";
			goto fail;
		}
		const char *colon = memchr(ls, ':', ll);
		if (!colon || colon == ls)
		{
			rc = 400;
			*why = "malformed header line";
			goto fail;
		}
		size_t nl = (size_t)(colon - ls);
		for (size_t i = 0; i < nl; i++)
			if (!is_tchar((unsigned char)ls[i]))
			{
				rc = 400;
				*why = "malformed header name";
				goto fail;
			}
		const char *v = colon + 1, *ve = ls + ll;
		while (v < ve && (*v == ' ' || *v == '\t'))
			v++;
		while (ve > v && (ve[-1] == ' ' || ve[-1] == '\t'))
			ve--;
		size_t vl = (size_t)(ve - v);

		// ищем нужные заголовки
		if (hdr_is(ls, nl, "Host"))
		{
			free(hosthdr);
			hosthdr = dupn(v, vl);
			if (!hosthdr)
			{
				rc = 500;
				*why = "out of memory";
				goto fail;
			}
		}
		else if (hdr_is(ls, nl, "Content-Length"))
		{
			if (vl == 0 || vl > 12)
			{
				rc = 400;
				*why = "bad Content-Length";
				goto fail;
			}
			size_t val = 0;
			for (size_t i = 0; i < vl; i++)
			{
				if (!isdigit((unsigned char)v[i]))
				{
					rc = 400;
					*why = "bad Content-Length";
					goto fail;
				}
				val = val * 10 + (size_t)(v[i] - '0');
			}
			if (have_cl && val != cl)
			{
				rc = 400;
				*why = "conflicting Content-Length";
				goto fail;
			}
			have_cl = 1;
			cl = val;
		}
		else if (hdr_is(ls, nl, "Transfer-Encoding"))
		{
			rc = 501;
			*why = "request Transfer-Encoding is not supported";
			goto fail;
		}
		else if (hdr_is(ls, nl, "Content-Type"))
		{
			free(r->ctype);
			r->ctype = dupn(v, vl);
			if (!r->ctype)
			{
				rc = 500;
				*why = "out of memory";
				goto fail;
			}
		}
	}

	// чужой метод - 501
	if (!known)
	{
		rc = 501;
		*why = "method not implemented";
		goto fail;
	}

	if (absolute)
	{
		r->host = auth_host;
		auth_host = NULL;
		strcpy(r->port, auth_port);
	}
	else
	{
		if (!hosthdr)
		{
			rc = 400;
			*why = "missing Host header";
			goto fail;
		}
		int e = parse_authority(hosthdr, strlen(hosthdr), &r->host, r->port);
		if (e == -2)
		{
			rc = 500;
			*why = "out of memory";
			goto fail;
		}
		if (e)
		{
			rc = 400;
			*why = "malformed Host header";
			goto fail;
		}
	}
	if (r->has_body_method && !have_cl)
	{
		rc = 400;
		*why = "Content-Length required";
		goto fail;
	}
	r->cl = have_cl ? cl : 0;

	// хост в нижний регистр + ключ кэша
	for (char *q = r->host; *q; q++)
		*q = (char)tolower((unsigned char)*q);
	if (asprintf(&r->key, "%s:%s%s", r->host, r->port, r->path) < 0)
	{
		r->key = NULL;
		rc = 500;
		*why = "out of memory";
		goto fail;
	}
	free(hosthdr);
	return 0;

fail:
	free(hosthdr);
	free(auth_host);
	request_free(r);
	return rc;
}

char *http_build_upstream(const request_t *r, size_t *len)
{
	char hostport[300];
	// хост с портом для запроса
	int v6 = strchr(r->host, ':') != NULL;
	snprintf(hostport, sizeof(hostport), "%s%s%s%s%s", v6 ? "[" : "", r->host, v6 ? "]" : "",
			 strcmp(r->port, "80") ? ":" : "", strcmp(r->port, "80") ? r->port : "");
	char *out = NULL;
	char cls[48] = "";
	if (r->has_body_method)
		snprintf(cls, sizeof(cls), "Content-Length: %zu\r\n", r->cl);
	int n = asprintf(&out, "%s %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n%s%s%s\r\n",
					 r->method, r->path, hostport,
					 r->ctype ? "Content-Type: " : "", r->ctype ? r->ctype : "", r->ctype ? "\r\n" : "");
	if (n < 0)
		return NULL;
	if (cls[0])
	{
		char *o2 = NULL;
		int n2 = asprintf(&o2, "%.*s%s\r\n", n - 2, out, cls);
		free(out);
		if (n2 < 0)
			return NULL;
		out = o2;
		n = n2;
	}
	*len = (size_t)n;
	return out;
}

int http_parse_response(const char *buf, size_t len, response_t *r, const char **why)
{
	memset(r, 0, sizeof(*r));
	const char *end = buf + len, *p = buf;
	size_t ll;
	// статус ответа
	const char *nx = next_line(p, end, &ll);
	if (!nx)
	{
		*why = "no status line";
		return -1;
	}
	if (ll < 12 || memcmp(buf, "HTTP/1.", 7) != 0 || (buf[7] != '0' && buf[7] != '1') || buf[8] != ' ')
	{
		*why = "malformed status line";
		return -1;
	}
	if (!isdigit((unsigned char)buf[9]) || !isdigit((unsigned char)buf[10]) || !isdigit((unsigned char)buf[11]))
	{
		*why = "malformed status code";
		return -1;
	}
	int st = (buf[9] - '0') * 100 + (buf[10] - '0') * 10 + (buf[11] - '0');
	if (st < 100 || st > 599)
	{
		*why = "status code out of range";
		return -1;
	}
	if (ll > 12 && buf[12] != ' ')
	{
		*why = "malformed status line";
		return -1;
	}
	r->status = st;
	const char *reason = ll > 13 ? buf + 13 : "";
	size_t rl = ll > 13 ? ll - 13 : 0;
	if (has_ctl(buf, ll))
	{
		*why = "control character in status line";
		return -1;
	}

	// собираем новые заголовки
	r->head = malloc(len * 2 + 64);
	if (!r->head)
	{
		*why = "out of memory";
		return -2;
	}
	size_t o = (size_t)snprintf(r->head, len * 2 + 64, "HTTP/1.0 %d %.*s\r\n", st, (int)(rl ? rl : 2), rl ? reason : "OK");
	p = nx;
	for (;;)
	{
		const char *ls = p;
		nx = next_line(p, end, &ll);
		if (!nx)
		{
			*why = "unterminated headers";
			goto bad;
		}
		p = nx;
		if (ll == 0)
			break;
		if (has_ctl(ls, ll))
		{
			*why = "control character in header";
			goto bad;
		}
		const char *colon = memchr(ls, ':', ll);
		if (!colon || colon == ls)
		{
			*why = "malformed header line";
			goto bad;
		}
		size_t nl = (size_t)(colon - ls);
		for (size_t i = 0; i < nl; i++)
			if (!is_tchar((unsigned char)ls[i]))
			{
				*why = "malformed header name";
				goto bad;
			}
		const char *v = colon + 1, *ve = ls + ll;
		while (v < ve && (*v == ' ' || *v == '\t'))
			v++;
		while (ve > v && (ve[-1] == ' ' || ve[-1] == '\t'))
			ve--;
		size_t vl = (size_t)(ve - v);

		// выкидываем лишние заголовки
		if (hdr_is(ls, nl, "Connection") || hdr_is(ls, nl, "Keep-Alive") ||
			hdr_is(ls, nl, "Proxy-Connection") || hdr_is(ls, nl, "Trailer") ||
			hdr_is(ls, nl, "Upgrade"))
			continue;
		if (hdr_is(ls, nl, "Transfer-Encoding"))
		{
			*why = "chunked/transfer-encoded response to an HTTP/1.0 request is not supported";
			goto bad;
		}
		// запоминаем длину
		if (hdr_is(ls, nl, "Content-Length"))
		{
			if (vl == 0 || vl > 18)
			{
				*why = "bad Content-Length";
				goto bad;
			}
			unsigned long long val = 0;
			for (size_t i = 0; i < vl; i++)
			{
				if (!isdigit((unsigned char)v[i]))
				{
					*why = "bad Content-Length";
					goto bad;
				}
				val = val * 10 + (unsigned)(v[i] - '0');
			}
			if (r->has_cl && val != r->cl)
			{
				*why = "conflicting Content-Length";
				goto bad;
			}
			r->has_cl = 1;
			r->cl = val;
		}
		if (hdr_is(ls, nl, "Content-Type") && !r->mime)
		{
			r->mime = dupn(v, vl);
			if (!r->mime)
			{
				*why = "out of memory";
				goto bad_mem;
			}
		}
		memcpy(r->head + o, ls, ll);
		o += ll;
		r->head[o++] = '\r';
		r->head[o++] = '\n';
	}
	// ставим свой Connection
	memcpy(r->head + o, "Connection: close\r\n\r\n", 21);
	o += 21;
	r->head[o] = 0;
	r->head_len = o;
	return 0;
bad:
	response_free(r);
	return -1;
bad_mem:
	response_free(r);
	return -2;
}
