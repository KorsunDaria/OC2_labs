#include "mythread.h"

#include <sched.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>

#define STACK_SIZE (1024 * 1024) 

typedef struct {
    void *(*start_routine)(void *);
    void  *arg;
    void  *retval;
} thread_ctx_t;

typedef struct node {
    mythread_t    tid;
    void         *stack;
    thread_ctx_t *ctx;
    struct node  *next;
} node_t;

static node_t *registry = NULL;
static int      lock     = 0;   

static void lock_acquire(void)
{
    while (__sync_lock_test_and_set(&lock, 1))
        ;
}

static void lock_release(void)
{
    __sync_lock_release(&lock);
}

static void registry_add(node_t *n)
{
    lock_acquire();
    n->next = registry;
    registry = n;
    lock_release();
}

static node_t *registry_take(mythread_t tid)
{
    lock_acquire();
    node_t **pp = &registry;
    while (*pp != NULL) {
        if ((*pp)->tid == tid) {
            node_t *n = *pp;
            *pp = n->next;
            lock_release();
            return n;
        }
        pp = &(*pp)->next;
    }
    lock_release();
    return NULL;
}

static int trampoline(void *arg)
{
    thread_ctx_t *ctx = (thread_ctx_t *)arg;
    ctx->retval = ctx->start_routine(ctx->arg);
    return 0;
}

int mythread_create(mythread_t *thread, void *(*start_routine)(void *), void *arg)
{
    if (thread == NULL || start_routine == NULL) {
        errno = EINVAL;
        return -1;
    }

    void *stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED)
        return -1;

    thread_ctx_t *ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        munmap(stack, STACK_SIZE);
        return -1;
    }
    ctx->start_routine = start_routine;
    ctx->arg            = arg;
    ctx->retval          = NULL;

    node_t *node = malloc(sizeof(*node));
    if (node == NULL) {
        free(ctx);
        munmap(stack, STACK_SIZE);
        return -1;
    }
    void *stack_top = (char *)stack + STACK_SIZE;

    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_SYSVSEM;

    pid_t tid = clone(trampoline, stack_top, flags, ctx);
    if (tid == -1) {
        int saved_errno = errno;
        free(node);
        free(ctx);
        munmap(stack, STACK_SIZE);
        errno = saved_errno;
        return -1;
    }

    node->tid   = tid;
    node->stack = stack;
    node->ctx   = ctx;
    registry_add(node);

    *thread = tid;
    return 0;
}

int mythread_join(mythread_t thread, void **retval)
{
    int status;

    if (waitpid(thread, &status, __WALL) == -1)
        return -1;

    node_t *node = registry_take(thread);
    if (node == NULL) {
        errno = ESRCH;  
        return -1;
    }

    if (retval != NULL)
        *retval = node->ctx->retval;

    munmap(node->stack, STACK_SIZE);
    free(node->ctx);
    free(node);

    return 0;
}