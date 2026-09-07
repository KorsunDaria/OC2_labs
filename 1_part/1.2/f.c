#include <stdio.h>
#include <pthread.h>

void *f(void *a) {
    printf("%lu\n", pthread_self());
    return NULL;
}

int main() {
    pthread_t t;
    pthread_attr_t at;
    long n = 0;

    pthread_attr_init(&at);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);

    while (1) {
        if (pthread_create(&t, &at, f, NULL) != 0) {
            printf("%ld\n", n);
            break;
        }
        n++;
    }
    return 0;
}

//ps -o pid,vsize,comm -p \$(pgrep my_f)"