#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

void *f(void *a) {
    sleep(2);
    printf("done\n");
    return NULL;
}

int main() {
    pthread_t t;
    int err;

    err = pthread_create(&t, NULL, f, NULL);
    if (err != 0) {
        fprintf(stderr, "pthread_create %s\n", strerror(err));
        return 1;
    }

    printf("%d\n", getpid());

    err = pthread_join(t, NULL);
    if (err != 0) {
        fprintf(stderr, "pthread_join %s\n", strerror(err));
        return 1;
    }

    return 0;
}