/*
 * ticket_booking.c — Week 1, Problem 4 (stretch): Multi-threaded simulation
 *
 * Scenario: a venue with a fixed number of seats. Many threads act as
 * booking agents, each trying to claim a batch of seats for a
 * customer. Each booking is a classic check-then-act:
 *
 *   1. check: is (seats_sold + requested) <= SEATS_AVAILABLE ?
 *   2. act:   seats_sold += requested
 *
 * Invariant that should ALWAYS hold: seats_sold <= SEATS_AVAILABLE.
 *
 * Between step 1 and step 2 of one thread, another thread can run its
 * own step 1 and step 2. Both checks can pass against the same stale
 * seats_sold, and both bookings go through — selling more seats than
 * exist. This mirrors 04_bank_chaos.c's check-then-act bug, applied
 * to a different domain, to show the bug is general.
 *
 * Compile:
 *   gcc -Wall -pthread ticket_booking.c -o ticket_booking
 *
 * Run several times:
 *   ./ticket_booking
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define SEATS_AVAILABLE     1000
#define NUM_AGENTS          8
#define BOOKINGS_PER_AGENT  500
#define SEATS_PER_BOOKING   3

/*
 * NOTE on WINDOW_USLEEP: with a bare check-then-act (no delay between
 * them, like the very first version of this file), the race window
 * is only a couple of CPU instructions wide. Across 15 test runs
 * with no delay, seats_sold landed on exactly 999 every single time
 * — the violation never manifested, for the same reason
 * 02_race_counter.c needs a huge ITERS count to show its bug (Week 1,
 * Problem 1). To reliably *observe* the violation (the point of this
 * problem), we widen the window with a tiny usleep between the check
 * and the act, the same amplification technique from Problem 2.
 */
#define WINDOW_USLEEP       5

int seats_sold = 0;   /* SHARED, unprotected on purpose */

typedef struct {
    int agent_id;
    int accepted;
    int rejected;
} agent_result_t;

void *agent(void *arg) {
    agent_result_t *r = (agent_result_t *)arg;
    r->accepted = 0;
    r->rejected = 0;

    for (int i = 0; i < BOOKINGS_PER_AGENT; i++) {
        /* check */
        if (seats_sold + SEATS_PER_BOOKING <= SEATS_AVAILABLE) {
            usleep(WINDOW_USLEEP);   /* widen the gap — see note above */
            /* act — another agent's check may have already passed
             * against the same seats_sold we just read */
            seats_sold += SEATS_PER_BOOKING;
            r->accepted++;
        } else {
            r->rejected++;
        }
    }
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_AGENTS];
    agent_result_t results[NUM_AGENTS];

    for (int i = 0; i < NUM_AGENTS; i++) {
        results[i].agent_id = i;
        pthread_create(&threads[i], NULL, agent, &results[i]);
    }

    int total_accepted = 0;
    for (int i = 0; i < NUM_AGENTS; i++) {
        pthread_join(threads[i], NULL);
        printf("Agent %d: %d accepted, %d rejected\n",
               results[i].agent_id, results[i].accepted, results[i].rejected);
        total_accepted += results[i].accepted;
    }

    printf("\nSeats available:        %d\n", SEATS_AVAILABLE);
    printf("Seats sold (reported):   %d\n", seats_sold);
    printf("Seats sold (accepted*%d): %d\n", SEATS_PER_BOOKING,
           total_accepted * SEATS_PER_BOOKING);

    if (seats_sold > SEATS_AVAILABLE) {
        printf("INVARIANT VIOLATED: sold %d seats but only %d exist! "
               "Overbooked by %d.\n",
               seats_sold, SEATS_AVAILABLE, seats_sold - SEATS_AVAILABLE);
    } else {
        printf("Invariant held this run (try again — it doesn't always).\n");
    }

    return 0;
}
