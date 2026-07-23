// procpeek_test.c — Week 5, Problem 3: Process peeker test
//
// Forks a child, has the PARENT sleep briefly so the child has time
// to actually start running, then calls getprocstate(childpid) from
// the parent and prints the result -- both the raw number and a
// human-readable name, since the enum procstate values aren't
// self-explanatory on their own. Also checks a pid that's
// essentially guaranteed not to exist.
//
// Place in xv6-riscv/user/, add $U/_procpeek_test\ to UPROGS.

#include "kernel/types.h"
#include "user/user.h"

// Mirrors enum procstate in kernel/proc.h:
// { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE }
char *state_names[] = {
  "UNUSED", "USED", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE"
};

char *
state_name(int state)
{
  if (state < 0 || state > 5) {
    return "???";
  }
  return state_names[state];
}

int
main(void)
{
  int pid, state;

  pid = fork();
  if (pid < 0) {
    printf("procpeek_test: fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // Child: just spin doing nothing in particular for a while so
    // there's a good chance the parent observes it as RUNNABLE or
    // RUNNING rather than having already exited.
    volatile int i;
    for (i = 0; i < 50000000; i++) {
      ;
    }
    exit(0);
  }

  // Parent: give the child a moment to actually get scheduled at
  // least once before we peek at it.
  sleep(2);

  state = getprocstate(pid);
  printf("Child PID %d is in state %d (%s)\n", pid, state, state_name(state));

  state = getprocstate(9999);
  printf("getprocstate(9999) [should not exist] -> %d\n", state);
  if (state == -1) {
    printf("PASS: nonexistent pid correctly reported as -1.\n");
  } else {
    printf("FAIL: expected -1 for a nonexistent pid.\n");
  }

  wait(0);
  exit(0);
}
