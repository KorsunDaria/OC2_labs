#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ucontext.h>

#define NTHREADS 5
#define STACK_SIZE (64 * 1024)

struct uthread {
    ucontext_t ctx;
    void *stack_base;       
    size_t stack_len;      
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
            swapcontext(&threads[prev]->ctx, &threads[next]->ctx);
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
    size_t page, len;
    void *base;

    if (thread == NULL || start_routine == NULL || nthreads > NTHREADS)
        return -1;
    if (getcontext(&thread->ctx) == -1)
        return -1;

    page = sysconf(_SC_PAGESIZE);
    len = STACK_SIZE + page;                   

    base = mmap(NULL, len, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (base == MAP_FAILED)
        return -1;

    if (mprotect(base, page, PROT_NONE) == -1) {
        munmap(base, len);
        return -1;
    }

    thread->stack_base = base;
    thread->stack_len = len;

    thread->ctx.uc_stack.ss_sp = (char *)base + page;
    thread->ctx.uc_stack.ss_size = STACK_SIZE;
    thread->ctx.uc_link = NULL;
    makecontext(&thread->ctx, uthread_start, 0);

    thread->start_routine = start_routine;
    thread->arg = arg;
    thread->finished = 0;
    threads[nthreads++] = thread;
    return 0;
}

void uthread_destroy(uthread_t thread)
{
    if (thread->stack_base) {
        munmap(thread->stack_base, thread->stack_len);
        thread->stack_base = NULL;
    }
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
    struct uthread tid[NTHREADS] = { 0 };
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

    for (i = 0; i < NTHREADS; i++)
        uthread_destroy(&tid[i]);

    return 0;
}