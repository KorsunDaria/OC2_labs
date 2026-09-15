#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void h(void *p) { free(p); }
void *f(void *a)
{
    printf("arg = %p\n", a);
    char *s = malloc(12);
    strcpy(s, "hello");
    int q = 1;
    pthread_cleanup_push(h, s);

    printf("%d\n", q);
    //pthread_exit(s);
    //while (1)
    //{
        //pthread_cleanup_pop(1);
        printf("%s\n", s);
        sleep(1);
    //}
    pthread_cleanup_pop(1);
    printf("%d\n", q);
    //return 0;
}
int main()
{

    char *s;
    pthread_t t;
    pthread_create(&t, 0, f, 0);
    sleep(3);
    pthread_cancel(t);
    pthread_join(t, (void **)&s);
    printf("s = %p\n", s);
}