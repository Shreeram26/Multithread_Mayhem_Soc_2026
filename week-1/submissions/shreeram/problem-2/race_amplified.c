/*
 * race_amplified.c — Week 1, Problem 2: Make the race worse
 *
 * Starting point: 02_race_counter.c. That version uses `counter++`,
 * which the compiler can turn into a load/add/store that sometimes
 * happens to interleave badly — but the window between the load and
 * the store is only a couple of instructions wide, so on a modern
 * CPU the race doesn't "win" very often relative to the huge number
 * of iterations.
 *
 * The change here: split counter++ into its three logical steps by
 * hand (load into tmp, do something else, add 1, store tmp back),
 * and deliberately widen the gap between the load and the store with
 * a tiny usleep(). That gives the OS scheduler plenty of opportunity
 * to switch to the other thread mid-increment, so both threads load
 * the same stale value far more often.
 *
 * We do NOT use printf inside the racy section — printf takes an
 * internal lock (stdio streams are thread-safe by default in glibc),
 * which would itself act as accidental synchronization and mask the
 * very race we're trying to amplify.
 *
 * Compile:
 *   gcc -Wall -pthread race_amplified.c -o race_amplified
 *
 * Run:
 *   ./race_amplified
 */

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#define ITERS 2000

long counter = 0;

void *increment(void *arg) {
    (void)arg;
    for (long i = 0; i < ITERS; i++) {
        long tmp = counter;   /* 1. load */
        usleep(1);             /* 2. widen the gap on purpose */
        tmp = tmp + 1;          /*    (do the "add" after the sleep) */
        counter = tmp;          /* 3. store — may stomp the other thread */
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;

    pthread_create(&t1, NULL, increment, NULL);
    pthread_create(&t2, NULL, increment, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    long expected = 2L * ITERS;
    printf("Final counter:  %ld\n", counter);
    printf("Expected:       %ld\n", expected);
    printf("Difference:     %ld\n", expected - counter);

    if (counter == expected) {
        printf("Result: CORRECT (rare with the amplified version — try again)\n");
    } else {
        printf("Result: WRONG by %ld — race amplified successfully.\n", expected - counter);
    }

    return 0;
}
