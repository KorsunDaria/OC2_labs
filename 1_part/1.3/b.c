#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct { int a; char *b; } S;

void *f(void *p) {
    S *s = (S*)p;
    printf("%d %s\n", s->a, s->b);
    free(s);
    return NULL;
}

int main() {
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);

    S *s = malloc(sizeof(S));
    s->a = 1; 
    s->b = "a";

    pthread_t t;
    pthread_create(&t, &at, f, s);
    pthread_attr_destroy(&at);
     
    pthread_exit(NULL);
}