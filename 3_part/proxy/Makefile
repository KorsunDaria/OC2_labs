CC      ?= gcc
CFLAGS  = -std=gnu11 -O2 -g -Wall -Wextra -pedantic -D_GNU_SOURCE -pthread
SRC     = src/main.c src/log.c src/pqueue.c src/http.c src/cache.c src/fetcher.c src/worker.c
HDR     = $(wildcard src/*.h)
BIN     = proxy

all: $(BIN)

$(BIN): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $@

tsan: proxy_tsan
proxy_tsan: $(SRC) $(HDR)
	$(CC) -std=gnu11 -O1 -g -Wall -Wextra -D_GNU_SOURCE -pthread -fsanitize=thread $(SRC) -o $@

asan: proxy_asan
proxy_asan: $(SRC) $(HDR)
	$(CC) -std=gnu11 -O1 -g -Wall -Wextra -D_GNU_SOURCE -pthread -fsanitize=address,undefined $(SRC) -o $@

clean:
	rm -f $(BIN) proxy_tsan proxy_asan

.PHONY: all tsan asan clean
