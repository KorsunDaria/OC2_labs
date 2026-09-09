#include <stdio.h>
#include <pthread.h>

void *f(void *a) {
    printf("arg = %p\n", a);
    return (void *) 42;
}

int main() {
    pthread_t t;
    void *r;
    pthread_create(&t, NULL, f, NULL);
    pthread_join(t, &r);
    printf("%p\n", r);
    return 0;
}