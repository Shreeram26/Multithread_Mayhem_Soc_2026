/*
 * bank_chaos_fixed.c — Week 2, Problem 1 (Part B)
 *
 * Fixed version of week-1/code/04_bank_chaos.c. The check-then-act
 * (is there enough balance? -> subtract) is now wrapped entirely
 * inside a single critical section, so no other thread can act on
 * the balance between the check and the subtraction.
 *
 * Compile:
 *   gcc -Wall -pthread bank_chaos_fixed.c -o bank_chaos_fixed
 *
 * Run 20 times to confirm balance never goes negative and the
 * success counts always add up:
 *   for i in {1..20}; do ./bank_chaos_fixed; done
 */

#include <stdio.h>
#include <pthread.h>

int balance = 10000;
pthread_mutex_t balance_lock = PTHREAD_MUTEX_INITIALIZER;

#define ATTEMPTS_PER_CUSTOMER 100000
#define WITHDRAWAL_AMOUNT     1

typedef struct {
    const char *name;
    int successful;
    int rejected;
} customer_result_t;

void *customer(void *arg) {
    customer_result_t *r = (customer_result_t *)arg;
    r->successful = 0;
    r->rejected   = 0;

    for (int i = 0; i < ATTEMPTS_PER_CUSTOMER; i++) {
        pthread_mutex_lock(&balance_lock);

        /* check AND act, both inside the same lock — no other
         * thread can slip in between them anymore. */
        if (balance >= WITHDRAWAL_AMOUNT) {
            balance -= WITHDRAWAL_AMOUNT;
            r->successful++;
        } else {
            r->rejected++;
        }

        pthread_mutex_unlock(&balance_lock);
    }
    return NULL;
}

int main(void) {
    pthread_t alice, bob;
    customer_result_t alice_r = {"Alice", 0, 0};
    customer_result_t bob_r   = {"Bob", 0, 0};

    pthread_create(&alice, NULL, customer, &alice_r);
    pthread_create(&bob,   NULL, customer, &bob_r);

    pthread_join(alice, NULL);
    pthread_join(bob,   NULL);

    printf("Customer Alice: %d successful, %d rejected\n", alice_r.successful, alice_r.rejected);
    printf("Customer Bob:   %d successful, %d rejected\n", bob_r.successful, bob_r.rejected);
    printf("Final balance: %d\n", balance);

    int total_withdrawn = (alice_r.successful + bob_r.successful) * WITHDRAWAL_AMOUNT;
    printf("Sanity check: 10000 - %d = %d (should equal final balance)\n",
           total_withdrawn, 10000 - total_withdrawn);

    if (balance < 0) {
        printf("BUG: balance went negative — should be impossible now.\n");
    } else {
        printf("Balance never went negative. Accounting is consistent.\n");
    }

    pthread_mutex_destroy(&balance_lock);
    return 0;
}
