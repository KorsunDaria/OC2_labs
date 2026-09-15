#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

void handler(int signo) {
    (void)signo;
    write(1, "SIGINT поймал обработчик\n", 41); 
}

void *blocker(void *a) {
    (void)a;
    sigset_t all;
    sigfillset(&all);
    pthread_sigmask(SIG_BLOCK, &all, NULL);
    while (1) sleep(1);
    return NULL;
}

void *sigint_thread(void *a) {
    (void)a;
    sigset_t s;
    sigemptyset(&s);
    sigaddset(&s, SIGINT);
    
    signal(SIGINT, handler);
    pthread_sigmask(SIG_UNBLOCK, &s, NULL);

    while (1) pause();
    return NULL;
}

void *sigquit_thread(void *a) {
    (void)a;
    sigset_t s;
    int signo;
    sigemptyset(&s);
    sigaddset(&s, SIGQUIT);
    while (1) {
        sigwait(&s, &signo);
        printf("получен SIGQUIT\n");
        fflush(stdout); 
    }
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0); 

    pthread_t t1, t2, t3;
    sigset_t s;

    sigemptyset(&s);
    sigaddset(&s, SIGINT);
    sigaddset(&s, SIGQUIT);
    pthread_sigmask(SIG_BLOCK, &s, NULL); 

    int rc;

    rc = pthread_create(&t1, NULL, blocker, NULL);
    if (rc != 0) { fprintf(stderr, "pthread_create t1: %s\n", strerror(rc)); return 1; }

    rc = pthread_create(&t2, NULL, sigint_thread, NULL);
    if (rc != 0) { fprintf(stderr, "pthread_create t2: %s\n", strerror(rc)); return 1; }

    rc = pthread_create(&t3, NULL, sigquit_thread, NULL);
    if (rc != 0) { fprintf(stderr, "pthread_create t3: %s\n", strerror(rc)); return 1; }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    return 0;
}