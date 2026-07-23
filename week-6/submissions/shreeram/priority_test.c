// priority_test.c — Week 6, Problem 2: Priority scheduler test
//
// Forks two children, sets one to a high priority (5) and the other
// to a low priority (50), lets both busy-loop for the same fixed
// window of ticks (agreed via a deadline computed BEFORE forking,
// so both children start from the identical target), and reports
// how many iterations each completed.
//
// Place in xv6-riscv/user/, add $U/_priority_test\ to UPROGS.

#include "kernel/types.h"
#include "user/user.h"

#define NCHILD   2
#define DURATION 100   // ticks

int
main(void)
{
  int pipefds[NCHILD][2];
  int children[NCHILD];
  int prios[NCHILD] = {5, 50};
  int start, deadline;
  int i, pid, old;
  uint64 counts[NCHILD];

  start = uptime();
  deadline = start + DURATION;

  for (i = 0; i < NCHILD; i++) {
    if (pipe(pipefds[i]) < 0) {
      printf("priority_test: pipe failed\n");
      exit(1);
    }

    pid = fork();
    if (pid < 0) {
      printf("priority_test: fork failed\n");
      exit(1);
    }

    if (pid == 0) {
      // Child: CPU-bound busy loop until the shared deadline.
      close(pipefds[i][0]);
      volatile uint64 count = 0;
      while (uptime() < deadline) {
        count++;
        __sync_synchronize();   // stop the compiler optimizing the loop away
      }
      write(pipefds[i][1], (char *)&count, sizeof(count));
      close(pipefds[i][1]);
      exit(0);
    }

    // Parent: now that we know this child's pid, set its priority.
    children[i] = pid;
    close(pipefds[i][1]);
    old = set_priority(pid, prios[i]);
    printf("Parent: set child %d (PID %d) to priority %d (was %d)\n",
           i, pid, prios[i], old);
  }

  for (i = 0; i < NCHILD; i++) {
    read(pipefds[i][0], (char *)&counts[i], sizeof(counts[i]));
    close(pipefds[i][0]);
    wait(0);
  }

  for (i = 0; i < NCHILD; i++) {
    printf("Child %d (PID %d, priority %d): completed %lu iterations\n",
           i, children[i], prios[i], counts[i]);
  }

  if (counts[1] > 0) {
    printf("Child 0 ran about %ldx more than child 1\n",
           (long)(counts[0] / counts[1]));
  }

  exit(0);
}
