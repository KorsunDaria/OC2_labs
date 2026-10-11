CC=gcc
RM=rm
CFLAGS= -g -Wall -I.
LIBS=-lpthread

VARIANTS = spin mutex cond sem
# queue-<v>       : tasks a, b, c (e, f, g for mutex, cond, sem)
# queue-<v>-sleep : same, but writer calls usleep(1) (task d)
BINS = $(foreach v,$(VARIANTS),queue-$(v) queue-$(v)-sleep)

all: queue-example $(BINS)

queue-example: queue.h queue-example.c queue-spin.c queue-common.c
	${CC} ${CFLAGS} queue-example.c queue-spin.c queue-common.c ${LIBS} -o $@

$(foreach v,$(VARIANTS),queue-$(v)): queue-%: queue.h queue-threads.c queue-%.c queue-common.c
	${CC} ${CFLAGS} queue-threads.c queue-$*.c queue-common.c ${LIBS} -o $@

$(foreach v,$(VARIANTS),queue-$(v)-sleep): queue-%-sleep: queue.h queue-threads.c queue-%.c queue-common.c
	${CC} ${CFLAGS} -DWRITER_USLEEP queue-threads.c queue-$*.c queue-common.c ${LIBS} -o $@

clean:
	${RM} -f *.o queue-example $(BINS)
