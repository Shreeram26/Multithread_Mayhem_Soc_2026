/*
 * rw_lock.c — Week 3, Problem 3: Readers-writers from scratch
 *
 * A readers-writers lock built from a plain mutex + two condition
 * variables (NOT pthread_rwlock_t — that would defeat the point of
 * the exercise). Multiple readers may hold the lock at once; a
 * writer needs exclusive access.
 *
 * Policy chosen: WRITER PREFERENCE. A new reader that arrives while
 * any writer is either active OR waiting will block, even if the
 * lock is technically free for reading right now. This prevents
 * writer starvation (a steady stream of readers could otherwise keep
 * a writer waiting forever), at the cost of readers being able to
 * starve under a constant stream of writers. This is documented here
 * rather than hidden, per the assignment's instructions.
 *
 * State:
 *   active_readers   — readers currently holding the lock
 *   active_writers    — 0 or 1, a writer currently holding the lock
 *   waiting_writers   — writers blocked waiting for their turn
 *
 * Compile:
 *   gcc -Wall -pthread -std=c11 rw_lock.c -o rw_lock
 *
 * Run:
 *   ./rw_lock
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  readers_ok;
    pthread_cond_t  writers_ok;
    int active_readers;
    int active_writers;
    int waiting_writers;
} rwlock_t;

static rwlock_t rw;

void rw_init(void) {
    pthread_mutex_init(&rw.mutex, NULL);
    pthread_cond_init(&rw.readers_ok, NULL);
    pthread_cond_init(&rw.writers_ok, NULL);
    rw.active_readers  = 0;
    rw.active_writers  = 0;
    rw.waiting_writers  = 0;
}

void reader_lock(void) {
    pthread_mutex_lock(&rw.mutex);
    /* Writer preference: block if a writer is active OR waiting. */
    while (rw.active_writers > 0 || rw.waiting_writers > 0) {
        pthread_cond_wait(&rw.readers_ok, &rw.mutex);
    }
    rw.active_readers++;
    pthread_mutex_unlock(&rw.mutex);
}

void reader_unlock(void) {
    pthread_mutex_lock(&rw.mutex);
    rw.active_readers--;
    if (rw.active_readers == 0) {
        /* Last reader out — wake exactly one waiting writer, if any. */
        pthread_cond_signal(&rw.writers_ok);
    }
    pthread_mutex_unlock(&rw.mutex);
}

void writer_lock(void) {
    pthread_mutex_lock(&rw.mutex);
    rw.waiting_writers++;
    while (rw.active_writers > 0 || rw.active_readers > 0) {
        pthread_cond_wait(&rw.writers_ok, &rw.mutex);
    }
    rw.waiting_writers--;
    rw.active_writers = 1;
    pthread_mutex_unlock(&rw.mutex);
}

void writer_unlock(void) {
    pthread_mutex_lock(&rw.mutex);
    rw.active_writers = 0;
    /* Wake a waiting writer first (writer preference); if none,
     * readers_ok's broadcast lets every blocked reader re-check. */
    pthread_cond_signal(&rw.writers_ok);
    pthread_cond_broadcast(&rw.readers_ok);
    pthread_mutex_unlock(&rw.mutex);
}

void rw_destroy(void) {
    pthread_mutex_destroy(&rw.mutex);
    pthread_cond_destroy(&rw.readers_ok);
    pthread_cond_destroy(&rw.writers_ok);
}

/* ---------- Stress test ---------- */

#define DB_SIZE        10
#define NUM_READERS     6
#define NUM_WRITERS     2
#define OPS_PER_READER  5
#define OPS_PER_WRITER  3

int db[DB_SIZE];

/* Diagnostic counters, protected by the same rwlock's mutex is
 * overkill for a demo, so we use a separate small lock just for
 * these stats to avoid interfering with the rwlock logic itself. */
pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;
int max_concurrent_readers_seen = 0;
int writer_reader_overlap_detected = 0;

static struct timespec prog_start;
static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec - prog_start.tv_sec) * 1000 + (ts.tv_nsec - prog_start.tv_nsec) / 1000000;
}

void *reader(void *arg) {
    int id = *(int *)arg;
    for (int op = 0; op < OPS_PER_READER; op++) {
        usleep(rand() % 30000 + 5000);

        reader_lock();
        pthread_mutex_lock(&stats_lock);
        if (rw.active_readers > max_concurrent_readers_seen) {
            max_concurrent_readers_seen = rw.active_readers;
        }
        pthread_mutex_unlock(&stats_lock);

        long t = now_ms();
        int val = db[id % DB_SIZE];
        usleep(20000);   /* simulate reading time */
        printf("[reader %d] t=%ld read db[%d]=%d  (concurrent readers now: %d)\n",
               id, t, id % DB_SIZE, val, rw.active_readers);

        reader_unlock();
    }
    return NULL;
}

void *writer(void *arg) {
    int id = *(int *)arg;
    for (int op = 0; op < OPS_PER_WRITER; op++) {
        usleep(rand() % 60000 + 20000);

        writer_lock();
        long t = now_ms();
        db[id % DB_SIZE]++;
        usleep(30000);   /* simulate writing time */
        printf("[WRITER %d] t=%ld wrote db[%d]=%d  (active_readers should be 0: %d)\n",
               id, t, id % DB_SIZE, db[id % DB_SIZE], rw.active_readers);
        if (rw.active_readers != 0) {
            writer_reader_overlap_detected = 1;
        }
        writer_unlock();
    }
    return NULL;
}

int main(void) {
    clock_gettime(CLOCK_MONOTONIC, &prog_start);
    srand(7);
    rw_init();
    for (int i = 0; i < DB_SIZE; i++) db[i] = i * 10;

    pthread_t readers[NUM_READERS], writers[NUM_WRITERS];
    int r_ids[NUM_READERS], w_ids[NUM_WRITERS];

    for (int i = 0; i < NUM_READERS; i++) {
        r_ids[i] = i;
        pthread_create(&readers[i], NULL, reader, &r_ids[i]);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        w_ids[i] = i;
        pthread_create(&writers[i], NULL, writer, &w_ids[i]);
    }
    for (int i = 0; i < NUM_READERS; i++) pthread_join(readers[i], NULL);
    for (int i = 0; i < NUM_WRITERS; i++) pthread_join(writers[i], NULL);

    printf("\nMax concurrent readers observed: %d (>1 means readers really overlapped)\n",
           max_concurrent_readers_seen);
    printf("Writer/reader overlap detected: %s (should be 'no')\n",
           writer_reader_overlap_detected ? "YES -- BUG" : "no");
    printf("Program did not deadlock (reached this line).\n");

    rw_destroy();
    return 0;
}
