// lock_detective.c — Week 6, Problem 3: Lock detective test
//
// Calls is_lock_held(0) [tickslock], is_lock_held(1) [cons.lock],
// and is_lock_held(99) [invalid], then sleeps a few ticks and checks
// tickslock again to see if the result ever differs.
//
// Place in xv6-riscv/user/, add $U/_lock_detective\ to UPROGS.

#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  int t0, c0, inv, t1;

  t0 = is_lock_held(0);
  printf("tickslock (id 0): %d (%s)\n", t0, t0 ? "held" : "not held");

  c0 = is_lock_held(1);
  printf("cons.lock (id 1): %d (%s)\n", c0, c0 ? "held" : "not held");

  inv = is_lock_held(99);
  printf("invalid (id 99):  %d\n", inv);

  sleep(5);

  t1 = is_lock_held(0);
  printf("After sleeping: tickslock (id 0): %d (%s)\n", t1, t1 ? "held" : "not held");

  if (t0 != t1) {
    printf("Result differed across the two checks.\n");
  } else {
    printf("Result was the same both times -- may just mean we got unlucky\n");
    printf("with timing both times, not that the lock is never contended.\n");
  }

  exit(0);
}
