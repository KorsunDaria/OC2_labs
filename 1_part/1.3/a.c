#include <stdio.h>
#include <pthread.h>

typedef struct { int a; char *b; } S;

void *f(void *p) {
    S *s = (S*)p;
    printf("%d %s\n", s->a, s->b);
    return NULL;
}

int main() {
    S s = {1, "a"};
    pthread_t t;
    pthread_create(&t, NULL, f, &s);
    pthread_join(t, NULL);
    return 0;
}