CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Werror -pthread
LDFLAGS ?= -pthread

.PHONY: all clean run

all: futex-sync

futex-sync: futex_sync.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

run: all
	./futex-sync none
	./futex-sync spin
	./futex-sync mutex

clean:
	rm -f futex-sync
