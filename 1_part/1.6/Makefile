CC       = gcc
CFLAGS   = -Wall -Wextra -Werror -std=gnu11 -D_GNU_SOURCE
AR       = ar
ARFLAGS  = rcs

LIB      = libmythread.a
LIBSRC   = mythread.c
LIBOBJ   = $(LIBSRC:.c=.o)

TESTBIN  = my_main
TESTBIN2 = my_main2

.PHONY: all clean

all: $(TESTBIN) $(TESTBIN2)

$(LIB): $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $^

$(TESTBIN): main.c $(LIB)
	$(CC) $(CFLAGS) main.c -o $@ -L. -lmythread

$(TESTBIN2): main2.c $(LIB)
	$(CC) $(CFLAGS) main2.c -o $@ -L. -lmythread

%.o: %.c mythread.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(LIB) $(LIBOBJ) $(TESTBIN) $(TESTBIN2)