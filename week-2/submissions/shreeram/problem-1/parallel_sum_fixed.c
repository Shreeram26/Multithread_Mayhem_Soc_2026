/*
 * parallel_sum_fixed.c — Week 2, Problem 1 (Part A)
 *
 * Fixed version of week-1/code/03_parallel_sum.c. The only change:
 * a pthread_mutex_t now protects every access to the shared `total`.
 * Each thread still computes its partial sum the same way, but the
 * `total += ...` critical section is now atomic with respect to the
 * other threads.
 *
 * Compile:
 *   gcc -Wall -pthread parallel_sum_fixed.c -o parallel_sum_fixed
 *
 * Run 20 times to confirm it's always correct:
 *   for i in {1..20}; do ./parallel_sum_fixed; done
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define ARRAY_SIZE  10000000
#define NUM_THREADS 4

int *array;
long total = 0;
pthread_mutex_t total_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    long start_index;
    long end_index;
} thread_arg_t;

void *partial_sum(void *arg) {
    thread_arg_t *t = (thread_arg_t *)arg;

    /* Compute the partial sum into a LOCAL variable first — no
     * locking needed here, nothing is shared yet. */
    long local = 0;
    for (long i = t->start_index; i < t->end_index; i++) {
        local += array[i];
    }

    /* Only the merge into the shared total needs the lock. This
     * keeps the critical section tiny — one lock/unlock per thread,
     * not once per array element. */
    pthread_mutex_lock(&total_lock);
    total += local;
    pthread_mutex_unlock(&total_lock);

    return NULL;
}

int main(void) {
    array = malloc(ARRAY_SIZE * sizeof(int));
    if (!array) {
        perror("malloc");
        return 1;
    }
    for (long i = 0; i < ARRAY_SIZE; i++) {
        array[i] = 1;
    }

    pthread_t threads[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];
    long chunk = ARRAY_SIZE / NUM_THREADS;

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].start_index = i * chunk;
        args[i].end_index   = (i == NUM_THREADS - 1) ? ARRAY_SIZE
                                                      : (i + 1) * chunk;
        pthread_create(&threads[i], NULL, partial_sum, &args[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Computed sum: %ld\n", total);
    printf("Expected:     %d\n",  ARRAY_SIZE);
    printf("%s\n", (total == ARRAY_SIZE) ? "CORRECT" : "WRONG — this should never happen now");

    pthread_mutex_destroy(&total_lock);
    free(array);
    return 0;
}
