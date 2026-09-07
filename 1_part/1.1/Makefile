CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu99 -pthread
SRCS = a.c b.c c.c d.c e.c
TARGETS = $(SRCS:.c=)

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

asan: CFLAGS += -fsanitize=address -fsanitize=leak -g
asan: clean all

clean:
	rm -f $(TARGETS)

.PHONY: all clean asan
