/*
 * safe_list.c — Week 2, Problem 3: Thread-safe linked list
 *
 * A singly-linked list of ints protected by ONE coarse-grained lock
 * for the whole list (as recommended — fine-grained per-node locking
 * is its own subtle topic, deliberately out of scope this week).
 *
 * API:
 *   void list_init(void);
 *   void list_insert(int value);
 *   int  list_contains(int value);
 *   void list_remove(int value);
 *   void list_destroy(void);
 *
 * The lock lives next to the head pointer, not inside each node —
 * that's the "common pitfall" the assignment calls out. A per-node
 * lock wouldn't protect the head pointer itself, or multi-node
 * operations like insert/remove.
 *
 * Stress test: 8 threads, 1000 mixed operations each, on a small
 * value range (0-99) so threads frequently collide on the same
 * values.
 *
 * Compile:
 *   gcc -Wall -pthread -std=c11 safe_list.c -o safe_list
 *
 * Run:
 *   ./safe_list
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct node {
    int value;
    struct node *next;
} node_t;

static node_t *head = NULL;
static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;

void list_init(void) {
    head = NULL;
}

void list_insert(int value) {
    node_t *n = malloc(sizeof(node_t));
    n->value = value;

    pthread_mutex_lock(&list_lock);
    n->next = head;
    head = n;
    pthread_mutex_unlock(&list_lock);
}

int list_contains(int value) {
    pthread_mutex_lock(&list_lock);
    node_t *cur = head;
    while (cur != NULL) {
        if (cur->value == value) {
            pthread_mutex_unlock(&list_lock);
            return 1;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&list_lock);
    return 0;
}

void list_remove(int value) {
    pthread_mutex_lock(&list_lock);
    node_t *cur = head, *prev = NULL;
    while (cur != NULL) {
        if (cur->value == value) {
            if (prev == NULL) {
                head = cur->next;
            } else {
                prev->next = cur->next;
            }
            free(cur);
            pthread_mutex_unlock(&list_lock);
            return;   /* remove only the first match */
        }
        prev = cur;
        cur = cur->next;
    }
    pthread_mutex_unlock(&list_lock);
}

void list_destroy(void) {
    pthread_mutex_lock(&list_lock);
    node_t *cur = head;
    while (cur != NULL) {
        node_t *next = cur->next;
        free(cur);
        cur = next;
    }
    head = NULL;
    pthread_mutex_unlock(&list_lock);
}

/* ---------- Stress test ---------- */

#define NUM_THREADS      8
#define OPS_PER_THREAD 1000
#define VALUE_RANGE      100

/* A tiny xorshift PRNG, used instead of rand_r() so this file has no
 * dependency on a specific libc's POSIX extensions — each thread
 * keeps its own seed on its own stack, so no locking is needed here. */
unsigned int next_rand(unsigned int *state) {
    unsigned int x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

void *worker(void *arg) {
    unsigned int seed = (unsigned int)(size_t)arg;

    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int value = next_rand(&seed) % VALUE_RANGE;
        int op    = next_rand(&seed) % 3;

        switch (op) {
            case 0: list_insert(value); break;
            case 1: list_contains(value); break;
            case 2: list_remove(value); break;
        }
    }
    return NULL;
}

/* Walks the list end-to-end to confirm it's well-formed: no cycles,
 * terminates at NULL. Only safe to call when no other thread is
 * touching the list (i.e., after all workers have joined). */
long list_length_and_check(void) {
    long len = 0;
    node_t *cur = head;
    /* Cap the walk so a corrupted (cyclic) list can't hang the test
     * forever — if we ever exceed a generous bound, something is
     * badly wrong. */
    long max_possible = (long)NUM_THREADS * OPS_PER_THREAD + 1;
    while (cur != NULL && len <= max_possible) {
        len++;
        cur = cur->next;
    }
    if (len > max_possible) {
        fprintf(stderr, "ERROR: list appears to be cyclic / corrupted!\n");
        exit(1);
    }
    return len;
}

int main(void) {
    list_init();

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, (void *)(size_t)(i + 1));
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All %d threads finished %d ops each without crashing.\n",
           NUM_THREADS, OPS_PER_THREAD);

    long len = list_length_and_check();
    printf("List is well-formed. Final length: %ld\n", len);

    /* Targeted correctness check: insert a fresh, distinctive value
     * and confirm contains() finds it; confirm a value we know we
     * never inserted (out of VALUE_RANGE) is reported absent. */
    list_insert(999999);
    printf("contains(999999) after inserting it: %d (expect 1)\n", list_contains(999999));
    printf("contains(-1) which was never inserted: %d (expect 0)\n", list_contains(-1));

    list_destroy();
    return 0;
}
