/*
 * sem_buffer.c — Week 3, Problem 2: Semaphore bounded buffer
 *
 * Rebuild of week-2's bounded_buffer.c, but using two counting
 * semaphores instead of two condition variables:
 *
 *   sem_t empty;  // counts free slots, starts at `capacity`
 *   sem_t full;   // counts filled slots, starts at 0
 *
 * buffer_put: sem_wait(empty) -> lock -> write -> unlock -> sem_post(full)
 * buffer_get: sem_wait(full)  -> lock -> read  -> unlock -> sem_post(empty)
 *
 * The mutex still protects the actual ring-buffer indices (`in`,
 * `out`, `data[]`) since multiple producers / multiple consumers can
 * still race on those even though the semaphores already handle the
 * "is there room / is there an item" waiting. Unlike Week 2, there's
 * no while-loop around the wait — the semaphore's count directly
 * encodes the condition, so there's nothing to re-check after
 * waking up.
 *
 * Compile:
 *   gcc -Wall -pthread -std=c11 sem_buffer.c -o sem_buffer
 *
 * Run:
 *   ./sem_buffer
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct {
    int *data;
    int capacity;
    int in, out;
    sem_t empty;
    sem_t full;
    pthread_mutex_t mutex;
} sem_buffer_t;

static sem_buffer_t sb;

void buffer_init(int capacity) {
    sb.data     = malloc(capacity * sizeof(int));
    sb.capacity = capacity;
    sb.in       = 0;
    sb.out      = 0;
    sem_init(&sb.empty, 0, capacity);   /* all slots free initially */
    sem_init(&sb.full,  0, 0);          /* no items yet */
    pthread_mutex_init(&sb.mutex, NULL);
}

void buffer_put(int item) {
    sem_wait(&sb.empty);                /* blocks here if full */
    pthread_mutex_lock(&sb.mutex);
    sb.data[sb.in] = item;
    sb.in = (sb.in + 1) % sb.capacity;
    pthread_mutex_unlock(&sb.mutex);
    sem_post(&sb.full);
}

int buffer_get(void) {
    sem_wait(&sb.full);                 /* blocks here if empty */
    pthread_mutex_lock(&sb.mutex);
    int item = sb.data[sb.out];
    sb.out = (sb.out + 1) % sb.capacity;
    pthread_mutex_unlock(&sb.mutex);
    sem_post(&sb.empty);
    return item;
}

void buffer_destroy(void) {
    free(sb.data);
    sem_destroy(&sb.empty);
    sem_destroy(&sb.full);
    pthread_mutex_destroy(&sb.mutex);
}

/* ---------- Test harness: 3 producers, 2 consumers ---------- */

#define NUM_PRODUCERS      3
#define NUM_CONSUMERS      2
#define ITEMS_PER_PRODUCER 1000
#define POISON_PILL        (-1)

typedef struct { int id; long count; long sum; } producer_arg_t;
typedef struct { long count; long sum; } consumer_arg_t;

pthread_mutex_t active_lock = PTHREAD_MUTEX_INITIALIZER;
int active_producers = NUM_PRODUCERS;

void *producer_fn(void *arg) {
    producer_arg_t *p = (producer_arg_t *)arg;
    p->count = 0;
    p->sum   = 0;
    int base = p->id * 1000;

    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        int value = base + i;
        buffer_put(value);
        p->count++;
        p->sum += value;
    }

    pthread_mutex_lock(&active_lock);
    active_producers--;
    int last = (active_producers == 0);
    pthread_mutex_unlock(&active_lock);

    if (last) {
        for (int i = 0; i < NUM_CONSUMERS; i++) buffer_put(POISON_PILL);
    }
    return NULL;
}

void *consumer_fn(void *arg) {
    consumer_arg_t *c = (consumer_arg_t *)arg;
    c->count = 0;
    c->sum   = 0;
    for (;;) {
        int item = buffer_get();
        if (item == POISON_PILL) break;
        c->count++;
        c->sum += item;
    }
    return NULL;
}

int main(void) {
    /* Run once with a tiny buffer, once with a large one, to check
     * both edge cases from the assignment. */
    int capacities[] = {1, 64};

    for (int trial = 0; trial < 2; trial++) {
        int capacity = capacities[trial];
        printf("=== Trial %d: buffer capacity = %d ===\n", trial + 1, capacity);

        buffer_init(capacity);
        active_producers = NUM_PRODUCERS;

        pthread_t producers[NUM_PRODUCERS], consumers[NUM_CONSUMERS];
        producer_arg_t p_args[NUM_PRODUCERS];
        consumer_arg_t c_args[NUM_CONSUMERS];

        for (int i = 0; i < NUM_PRODUCERS; i++) {
            p_args[i].id = i + 1;
            pthread_create(&producers[i], NULL, producer_fn, &p_args[i]);
        }
        for (int i = 0; i < NUM_CONSUMERS; i++) {
            pthread_create(&consumers[i], NULL, consumer_fn, &c_args[i]);
        }

        long total_produced_count = 0, total_produced_sum = 0;
        for (int i = 0; i < NUM_PRODUCERS; i++) {
            pthread_join(producers[i], NULL);
            total_produced_count += p_args[i].count;
            total_produced_sum   += p_args[i].sum;
        }
        long total_consumed_count = 0, total_consumed_sum = 0;
        for (int i = 0; i < NUM_CONSUMERS; i++) {
            pthread_join(consumers[i], NULL);
            total_consumed_count += c_args[i].count;
            total_consumed_sum   += c_args[i].sum;
        }

        printf("Produced: count=%ld sum=%ld\n", total_produced_count, total_produced_sum);
        printf("Consumed: count=%ld sum=%ld\n", total_consumed_count, total_consumed_sum);
        printf("%s\n\n", (total_produced_count == total_consumed_count &&
                           total_produced_sum   == total_consumed_sum)
                          ? "PASS" : "FAIL");

        buffer_destroy();
    }

    return 0;
}
