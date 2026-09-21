CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=gnu11
LDFLAGS = -pthread

SRCS    = $(wildcard *.c)
BINS    = $(patsubst %.c,%,$(SRCS))

.PHONY: all clean

all: $(BINS)

my_%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(BINS)