CC     = gcc
CFLAGS = -O2 -Wall -Wextra -pthread
LISTS  = list_pmutex list_pspin list_prwlock list_casspin list_fmutex

all: futex_sync $(LISTS)
futex_sync: futex_sync.c
	$(CC) $(CFLAGS) $< -o $@
list_pmutex:  list.c futex_lock.h ; $(CC) $(CFLAGS) -DUSE_MUTEX       $< -o $@
list_pspin:   list.c futex_lock.h ; $(CC) $(CFLAGS) -DUSE_SPIN        $< -o $@
list_prwlock: list.c futex_lock.h ; $(CC) $(CFLAGS) -DUSE_RWLOCK      $< -o $@
list_casspin: list.c futex_lock.h ; $(CC) $(CFLAGS) -DUSE_CAS_SPIN    $< -o $@
list_fmutex:  list.c futex_lock.h ; $(CC) $(CFLAGS) -DUSE_FUTEX_MUTEX $< -o $@
clean: ; rm -f futex_sync $(LISTS)