#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void *f(void *a) {
    printf("arg = %p\n", a);
    char *s = malloc(20);
    strcpy(s, "hello world");
    return (void *) s;
}

int main() {
    pthread_t t;
    void *r;
    pthread_create(&t, NULL, f, NULL);
    pthread_join(t, &r);
    printf("%s\n", (char *) r);
    free(r);
    return 0;
}