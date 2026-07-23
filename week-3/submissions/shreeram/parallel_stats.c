/*
 * parallel_stats.c — Week 3, Problem 1: Parallel statistics
 *
 * Splits a large array across several threads. Each thread computes
 * its own partial sum, min, max, and count into its OWN argument
 * struct (Approach A from week-3/code/02_return_values.c — no heap
 * allocation needed, no dangling pointers possible). The main thread
 * joins every worker and combines the partial results into sum, min,
 * max, and average for the whole array.
 *
 * Compile:
 *   gcc -Wall -pthread -std=c11 parallel_stats.c -o parallel_stats
 *
 * Run:
 *   ./parallel_stats
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>

#define ARRAY_SIZE   10000000
#define NUM_THREADS  4

typedef struct {
    /* inputs */
    int  *array;
    long  start, end;   /* [start, end) */

    /* outputs — written by the worker, read by main after join */
    long  partial_sum;
    int   partial_min;
    int   partial_max;
    long  partial_count;
} worker_result_t;

void *worker(void *arg) {
    worker_result_t *r = (worker_result_t *)arg;

    long sum = 0;
    int  mn  = INT_MAX;
    int  mx  = INT_MIN;
    long cnt = 0;

    for (long i = r->start; i < r->end; i++) {
        int v = r->array[i];
        sum += v;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        cnt++;
    }

    r->partial_sum   = sum;
    r->partial_min   = mn;
    r->partial_max   = mx;
    r->partial_count = cnt;
    return NULL;
}

int main(void) {
    int *array = malloc(ARRAY_SIZE * sizeof(int));
    if (!array) { perror("malloc"); return 1; }

    /* Fill with pseudo-random-ish values so min/max/avg are non-trivial. */
    srand(1234);
    for (long i = 0; i < ARRAY_SIZE; i++) {
        array[i] = (rand() % 2001) - 1000;   /* range: -1000..1000 */
    }

    pthread_t        threads[NUM_THREADS];
    worker_result_t   results[NUM_THREADS];
    long chunk = ARRAY_SIZE / NUM_THREADS;

    for (int i = 0; i < NUM_THREADS; i++) {
        results[i].array = array;
        results[i].start = i * chunk;
        results[i].end   = (i == NUM_THREADS - 1) ? ARRAY_SIZE : (i + 1) * chunk;
        pthread_create(&threads[i], NULL, worker, &results[i]);
    }

    long total_sum = 0, total_count = 0;
    int  global_min = INT_MAX, global_max = INT_MIN;

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);   /* no return value to capture — Approach A */
        total_sum   += results[i].partial_sum;
        total_count += results[i].partial_count;
        if (results[i].partial_min < global_min) global_min = results[i].partial_min;
        if (results[i].partial_max > global_max) global_max = results[i].partial_max;
        printf("Thread %d: sum=%-12ld min=%-6d max=%-6d count=%ld\n",
               i, results[i].partial_sum, results[i].partial_min,
               results[i].partial_max, results[i].partial_count);
    }

    double average = (double)total_sum / (double)total_count;

    printf("\nCombined statistics over %ld elements:\n", total_count);
    printf("  sum     = %ld\n", total_sum);
    printf("  min     = %d\n", global_min);
    printf("  max     = %d\n", global_max);
    printf("  average = %.4f\n", average);

    free(array);
    return 0;
}
