#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void h(void *p) { free(p); }
void *f(void *a)
{
    char *s = malloc(12);
    strcpy(s, "hello");
    pthread_cleanup_push(h, s);
    while (1)
    {
        printf("%s\n", s);
        sleep(1);
    }
    pthread_cleanup_pop(1);
    return 0;
}
int main()
{
    pthread_t t;
    pthread_create(&t, 0, f, 0);
    sleep(3);
    pthread_cancel(t);
    pthread_join(t, 0);
}