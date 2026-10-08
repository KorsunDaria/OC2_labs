#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <ucontext.h>
#include <signal.h>    

#define NTHREADS 5
#define STACK_SIZE (64 * 1024)

struct uthread {
    ucontext_t ctx;
    char stack[STACK_SIZE];
    void *(*start_routine)(void *);
    void *arg;
    int finished;
};

typedef struct uthread *uthread_t;

static struct uthread main_thread;
static uthread_t threads[NTHREADS + 1] = { &main_thread };
static int nthreads = 1;
static int current;

void uthread_yield(void)
{
    int prev = current;
    int i, next;

    for (i = 1; i < nthreads; i++) {
        next = (prev + i) % nthreads;
        if (!threads[next]->finished) {
            current = next;
            if (swapcontext(&threads[prev]->ctx, &threads[next]->ctx) == -1) {//ошибка
                perror("swapcontext");
                current = prev;
            }
            return;
        }
    }
}

static void uthread_start(void)
{
    uthread_t self = threads[current];

    self->start_routine(self->arg);
    self->finished = 1;
    uthread_yield();
}

int uthread_create(uthread_t thread, void *(*start_routine)(void *), void *arg)
{
    if (thread == NULL || start_routine == NULL || nthreads > NTHREADS)
        return -1;
    if (getcontext(&thread->ctx) == -1) { //ошибка 
        perror("getcontext");
        return -1;
    }

    thread->ctx.uc_stack.ss_sp = thread->stack;
    thread->ctx.uc_stack.ss_size = STACK_SIZE;
    thread->ctx.uc_link = NULL;

    if (STACK_SIZE < MINSIGSTKSZ) {
        fprintf(stderr, "makecontext: stack too small\n");
        return -1;
    }
    makecontext(&thread->ctx, uthread_start, 0); //ошибка 


    thread->start_routine = start_routine;
    thread->arg = arg;
    thread->finished = 0;
    threads[nthreads++] = thread;
    return 0;
}

void *mythread(void *arg)
{
    int i;

    printf("mythread %ld [%d %d %d]: Hello from mythread!\n",
           (long)arg, getpid(), getppid(), gettid());

    for (i = 0; i < 3; i++) {
        printf("mythread %ld: iteration %d\n", (long)arg, i);
        uthread_yield();
    }
    return NULL;
}

int main(void)
{
    struct uthread tid[NTHREADS];
    int i, done;

    printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());

    for (i = 0; i < NTHREADS; i++) {
        if (uthread_create(&tid[i], mythread, (void *)(long)i)) {
            printf("main: uthread_create() failed\n");
            return -1;
        }
    }

    do {
        uthread_yield();
        for (done = 1, i = 0; i < NTHREADS; i++)
            done &= tid[i].finished;
    } while (!done);

    return 0;
}