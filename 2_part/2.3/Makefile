CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Werror -pthread
LDFLAGS ?= -pthread

BINS = list-mutex list-spin list-rwlock

.PHONY: all clean run

all: $(BINS)

list-mutex: list.c
	$(CC) $(CFLAGS) -DUSE_MUTEX $< -o $@ $(LDFLAGS)

list-spin: list.c
	$(CC) $(CFLAGS) -DUSE_SPIN $< -o $@ $(LDFLAGS)

list-rwlock: list.c
	$(CC) $(CFLAGS) -DUSE_RWLOCK $< -o $@ $(LDFLAGS)


N ?= 1000
T ?= 5
run: all
	./list-mutex  $(N) $(T)
	./list-spin   $(N) $(T)
	./list-rwlock $(N) $(T)

clean:
	rm -f $(BINS)
