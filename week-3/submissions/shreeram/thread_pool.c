/*
 * thread_pool.c — Week 3, Problem 4: Thread pool
 *
 * A minimal but correct worker pool: a fixed set of threads pull
 * tasks off a shared FIFO queue (protected by a mutex + condition
 * variable) and run them. pool_wait() blocks the caller until every
 * submitted task has actually finished executing, using a second
 * condition variable signalled whenever the "in-flight" task count
 * drops to zero.
 *
 * API:
 *   void pool_init(int workers);
 *   void pool_submit(void (*fn)(void *), void *arg);
 *   void pool_wait(void);
 *   void pool_destroy(void);
 *
 * Compile:
 *   gcc -Wall -pthread -std=c11 thread_pool.c -o thread_pool
 *
 * Run:
 *   ./thread_pool
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct task {
    void (*fn)(void *);
    void *arg;
    struct task *next;
} task_t;

typedef struct {
    pthread_t   *workers;
    int          num_workers;

    task_t      *head, *tail;      /* FIFO task queue */
    pthread_mutex_t queue_lock;
    pthread_cond_t  task_available;

    int          pending;          /* tasks submitted but not yet finished */
    pthread_cond_t  all_done;

    int          shutting_down;
} thread_pool_t;

static thread_pool_t pool;

static void *worker_loop(void *arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&pool.queue_lock);

        while (pool.head == NULL && !pool.shutting_down) {
            pthread_cond_wait(&pool.task_available, &pool.queue_lock);
        }

        if (pool.head == NULL && pool.shutting_down) {
            pthread_mutex_unlock(&pool.queue_lock);
            break;   /* no more work will ever arrive — exit thread */
        }

        task_t *t = pool.head;
        pool.head = t->next;
        if (pool.head == NULL) pool.tail = NULL;

        pthread_mutex_unlock(&pool.queue_lock);

        /* Run the task OUTSIDE the lock — a slow task must not block
         * other workers from picking up other tasks. */
        t->fn(t->arg);
        free(t);

        pthread_mutex_lock(&pool.queue_lock);
        pool.pending--;
        if (pool.pending == 0) {
            pthread_cond_signal(&pool.all_done);
        }
        pthread_mutex_unlock(&pool.queue_lock);
    }
    return NULL;
}

void pool_init(int workers) {
    pool.workers     = malloc(workers * sizeof(pthread_t));
    pool.num_workers = workers;
    pool.head = pool.tail = NULL;
    pool.pending = 0;
    pool.shutting_down = 0;
    pthread_mutex_init(&pool.queue_lock, NULL);
    pthread_cond_init(&pool.task_available, NULL);
    pthread_cond_init(&pool.all_done, NULL);

    for (int i = 0; i < workers; i++) {
        pthread_create(&pool.workers[i], NULL, worker_loop, NULL);
    }
}

void pool_submit(void (*fn)(void *), void *arg) {
    task_t *t = malloc(sizeof(task_t));
    t->fn = fn;
    t->arg = arg;
    t->next = NULL;

    pthread_mutex_lock(&pool.queue_lock);
    pool.pending++;
    if (pool.tail == NULL) {
        pool.head = pool.tail = t;
    } else {
        pool.tail->next = t;
        pool.tail = t;
    }
    pthread_cond_signal(&pool.task_available);   /* wake one sleeping worker */
    pthread_mutex_unlock(&pool.queue_lock);
}

void pool_wait(void) {
    pthread_mutex_lock(&pool.queue_lock);
    while (pool.pending > 0) {
        pthread_cond_wait(&pool.all_done, &pool.queue_lock);
    }
    pthread_mutex_unlock(&pool.queue_lock);
}

void pool_destroy(void) {
    pthread_mutex_lock(&pool.queue_lock);
    pool.shutting_down = 1;
    pthread_cond_broadcast(&pool.task_available);   /* wake every idle worker */
    pthread_mutex_unlock(&pool.queue_lock);

    for (int i = 0; i < pool.num_workers; i++) {
        pthread_join(pool.workers[i], NULL);
    }

    free(pool.workers);
    pthread_mutex_destroy(&pool.queue_lock);
    pthread_cond_destroy(&pool.task_available);
    pthread_cond_destroy(&pool.all_done);
}

/* ---------- Test: many tiny tasks, verify every one ran exactly once ---------- */

#define NUM_TASKS 500

pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;
long completed_tasks = 0;
int  ran_flags[NUM_TASKS];

void my_task(void *arg) {
    int idx = *(int *)arg;
    ran_flags[idx] = 1;   /* each task writes only its OWN slot -- no race */

    pthread_mutex_lock(&counter_lock);
    completed_tasks++;
    pthread_mutex_unlock(&counter_lock);

    free(arg);
}

int main(void) {
    pool_init(4);

    for (int i = 0; i < NUM_TASKS; i++) {
        int *idx = malloc(sizeof(int));
        *idx = i;
        pool_submit(my_task, idx);
    }

    pool_wait();   /* block until all 500 tasks have finished */

    printf("Tasks submitted:  %d\n", NUM_TASKS);
    printf("Tasks completed:  %ld\n", completed_tasks);

    int all_ran = 1;
    for (int i = 0; i < NUM_TASKS; i++) {
        if (!ran_flags[i]) { all_ran = 0; break; }
    }
    printf("Every task ran exactly once: %s\n", all_ran ? "YES" : "NO");

    pool_destroy();
    return (completed_tasks == NUM_TASKS && all_ran) ? 0 : 1;
}
