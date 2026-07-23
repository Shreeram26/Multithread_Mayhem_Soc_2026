/*
 * bounded_buffer.c — Week 2, Problem 2: Thread-safe bounded buffer
 *
 * A generic, reusable bounded buffer built on top of the pattern in
 * week-2/code/03_producer_consumer.c. Unlike that example (which is
 * a fixed one-producer/one-consumer demo hardwired into main()),
 * this version exposes a small API that any number of producer and
 * consumer threads can call without knowing anything about mutexes
 * or condition variables:
 *
 *   void buffer_init(int capacity);
 *   void buffer_put(int item);   // blocks if full
 *   int  buffer_get(void);       // blocks if empty
 *   void buffer_destroy(void);
 *
 * Test harness: 3 producer threads and 2 consumer threads share one
 * buffer. Producer N produces N*1000 .. N*1000+999. After everything
 * finishes we check that the consumed count and sum match the
 * produced count and sum exactly.
 *
 * Compile:
 *   gcc -Wall -pthread -std=c11 bounded_buffer.c -o bounded_buffer
 *
 * Run:
 *   ./bounded_buffer
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* ---------- Generic bounded buffer ---------- */

typedef struct {
    int *data;
    int capacity;
    int count;
    int in, out;
    pthread_mutex_t lock;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;
} bounded_buffer_t;

static bounded_buffer_t bb;

void buffer_init(int capacity) {
    bb.data     = malloc(capacity * sizeof(int));
    bb.capacity = capacity;
    bb.count    = 0;
    bb.in       = 0;
    bb.out      = 0;
    pthread_mutex_init(&bb.lock, NULL);
    pthread_cond_init(&bb.not_full, NULL);
    pthread_cond_init(&bb.not_empty, NULL);
}

void buffer_put(int item) {
    pthread_mutex_lock(&bb.lock);
    while (bb.count == bb.capacity) {
        pthread_cond_wait(&bb.not_full, &bb.lock);
    }
    bb.data[bb.in] = item;
    bb.in = (bb.in + 1) % bb.capacity;
    bb.count++;
    pthread_cond_signal(&bb.not_empty);
    pthread_mutex_unlock(&bb.lock);
}

int buffer_get(void) {
    pthread_mutex_lock(&bb.lock);
    while (bb.count == 0) {
        pthread_cond_wait(&bb.not_empty, &bb.lock);
    }
    int item = bb.data[bb.out];
    bb.out = (bb.out + 1) % bb.capacity;
    bb.count--;
    pthread_cond_signal(&bb.not_full);
    pthread_mutex_unlock(&bb.lock);
    return item;
}

void buffer_destroy(void) {
    free(bb.data);
    pthread_mutex_destroy(&bb.lock);
    pthread_cond_destroy(&bb.not_full);
    pthread_cond_destroy(&bb.not_empty);
}

/* ---------- Test harness ---------- */

#define NUM_PRODUCERS      3
#define NUM_CONSUMERS      2
#define ITEMS_PER_PRODUCER 1000
/* A sentinel value producers push once they're all done, one per
 * consumer, so consumers know when to stop calling buffer_get(). */
#define POISON_PILL (-1)

typedef struct {
    int id;
    long produced_count;
    long produced_sum;
} producer_arg_t;

typedef struct {
    long consumed_count;
    long consumed_sum;
} consumer_arg_t;

pthread_mutex_t active_producers_lock = PTHREAD_MUTEX_INITIALIZER;
int active_producers = NUM_PRODUCERS;

void *producer_fn(void *arg) {
    producer_arg_t *p = (producer_arg_t *)arg;
    p->produced_count = 0;
    p->produced_sum   = 0;

    int base = p->id * 1000;
    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        int value = base + i;
        buffer_put(value);
        p->produced_count++;
        p->produced_sum += value;
    }

    /* Last producer to finish pushes one poison pill per consumer so
     * every consumer eventually sees a stop signal. */
    pthread_mutex_lock(&active_producers_lock);
    active_producers--;
    int last = (active_producers == 0);
    pthread_mutex_unlock(&active_producers_lock);

    if (last) {
        for (int i = 0; i < NUM_CONSUMERS; i++) {
            buffer_put(POISON_PILL);
        }
    }
    return NULL;
}

void *consumer_fn(void *arg) {
    consumer_arg_t *c = (consumer_arg_t *)arg;
    c->consumed_count = 0;
    c->consumed_sum   = 0;

    for (;;) {
        int item = buffer_get();
        if (item == POISON_PILL) break;
        c->consumed_count++;
        c->consumed_sum += item;
    }
    return NULL;
}

int main(void) {
    buffer_init(8);   /* small capacity on purpose to force blocking */

    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
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
        total_produced_count += p_args[i].produced_count;
        total_produced_sum   += p_args[i].produced_sum;
    }

    long total_consumed_count = 0, total_consumed_sum = 0;
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
        total_consumed_count += c_args[i].consumed_count;
        total_consumed_sum   += c_args[i].consumed_sum;
        printf("Consumer %d consumed %ld items, sum %ld\n",
               i, c_args[i].consumed_count, c_args[i].consumed_sum);
    }

    printf("\nProduced: count=%ld sum=%ld\n", total_produced_count, total_produced_sum);
    printf("Consumed: count=%ld sum=%ld\n", total_consumed_count, total_consumed_sum);

    if (total_produced_count == total_consumed_count &&
        total_produced_sum   == total_consumed_sum) {
        printf("PASS: counts and sums match exactly.\n");
    } else {
        printf("FAIL: mismatch detected — bug in the buffer implementation.\n");
    }

    buffer_destroy();
    return 0;
}
