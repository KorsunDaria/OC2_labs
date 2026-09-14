#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

void handler(int signo) {
    (void)signo;
    write(1, "SIGINT поймал обработчик\n", 26);
}

void *blocker(void *a) {           /* поток 1: блокирует все сигналы */
    (void)a;
    sigset_t all;
    sigfillset(&all);
    pthread_sigmask(SIG_BLOCK, &all, NULL);
    while (1) sleep(1);
    return NULL;
}

void *sigint_thread(void *a) {     /* поток 2: обработчик SIGINT */
    (void)a;
    sigset_t s;
    sigemptyset(&s);
    sigaddset(&s, SIGINT);
    pthread_sigmask(SIG_UNBLOCK, &s, NULL);
    signal(SIGINT, handler);
    while (1) pause();
    return NULL;
}

void *sigquit_thread(void *a) {    /* поток 3: sigwait на SIGQUIT */
    (void)a;
    sigset_t s;
    int signo;
    sigemptyset(&s);
    sigaddset(&s, SIGQUIT);
    while (1) {
        sigwait(&s, &signo);
        printf("получен SIGQUIT\n");
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2, t3;
    sigset_t s;

    sigemptyset(&s);
    sigaddset(&s, SIGINT);
    sigaddset(&s, SIGQUIT);
    pthread_sigmask(SIG_BLOCK, &s, NULL);   /* маска наследуется потоками */

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