/*
 * philosophers.c — Week 2, Problem 4 (stretch): Dining philosophers
 *
 * 5 philosophers, 5 forks (pthread_mutex_t), each philosopher needs
 * both adjacent forks to eat. Each philosopher eats at least
 * EAT_TARGET times before exiting.
 *
 * This file contains ONLY the FIXED version, built with lock
 * ordering: every philosopher acquires the lower-numbered fork
 * first, regardless of which side it's physically on. That removes
 * the circular-wait condition that causes the naive
 * "always left-then-right" version to deadlock. See REPORT.md for a
 * description of what the broken (left-then-right) version does
 * when run — all five threads print "picked up left fork" and then
 * hang forever, since every philosopher's right fork is some other
 * philosopher's left fork.
 *
 * Bonus (starvation): we count how many times each philosopher ate
 * and print the counts at the end so any imbalance is visible.
 *
 * Compile:
 *   gcc -Wall -pthread philosophers.c -o philosophers
 *
 * Run:
 *   ./philosophers
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define NUM_PHILOSOPHERS 5
#define EAT_TARGET       10

pthread_mutex_t forks[NUM_PHILOSOPHERS];
int meals_eaten[NUM_PHILOSOPHERS];

void think(int id) {
    (void)id;
    usleep(1000);
}

void eat(int id) {
    meals_eaten[id]++;
    usleep(1000);
}

void *philosopher(void *arg) {
    int id = *(int *)arg;
    int left  = id;
    int right = (id + 1) % NUM_PHILOSOPHERS;

    /* Lock ordering fix: always take the lower-numbered fork first.
     * This breaks the circular-wait cycle that the naive
     * "left then right" version creates. */
    int first  = (left < right) ? left  : right;
    int second = (left < right) ? right : left;

    while (meals_eaten[id] < EAT_TARGET) {
        think(id);

        pthread_mutex_lock(&forks[first]);
        pthread_mutex_lock(&forks[second]);

        printf("Philosopher %d is eating (meal #%d)\n", id, meals_eaten[id] + 1);
        eat(id);

        pthread_mutex_unlock(&forks[second]);
        pthread_mutex_unlock(&forks[first]);
    }

    printf("Philosopher %d is done — ate %d times.\n", id, meals_eaten[id]);
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_PHILOSOPHERS];
    int ids[NUM_PHILOSOPHERS];

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_mutex_init(&forks[i], NULL);
        meals_eaten[i] = 0;
        ids[i] = i;
    }

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_create(&threads[i], NULL, philosopher, &ids[i]);
    }
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nAll philosophers finished. Meal counts:\n");
    int min = meals_eaten[0], max = meals_eaten[0];
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        printf("  Philosopher %d: %d meals\n", i, meals_eaten[i]);
        if (meals_eaten[i] < min) min = meals_eaten[i];
        if (meals_eaten[i] > max) max = meals_eaten[i];
    }
    printf("Min meals: %d, Max meals: %d, Spread: %d\n", min, max, max - min);

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_mutex_destroy(&forks[i]);
    }
    return 0;
}
