// schedviz.c — Week 6, Problem 4 (stretch): Schedviz
//
// Forks N children (default 4), each busy-loops until a fixed
// deadline (in ticks) that every child agreed on BEFORE any fork
// happened, then reports its iteration count back to the parent,
// which prints a rough bar chart.
//
// NOTE on why this doesn't use a shared "stop flag": the assignment
// suggests a `volatile int` in an sbrk()-allocated page as a shared
// stop flag between parent and children. That does NOT actually
// work in this xv6 -- fork()'s uvmcopy() (kernel/vm.c) allocates a
// brand-new physical page with kalloc() and memmove()s the parent's
// page into it for the child. Parent and child end up with separate
// physical memory for every page, including anything from sbrk();
// writes in one are never visible in the other. (Stock xv6 has no
// mmap() and no genuine shared-memory mechanism between unrelated
// address spaces.) The actually-working pattern -- used by this
// course's own week-6/code/01_sched_trace.c -- is what's used here
// instead: agree on a deadline value *before* forking (so every
// child's copy of that already-computed number is correct from the
// start), and report results back through a pipe, which is real
// kernel-mediated communication, unlike raw shared memory.
//
// Usage: schedviz [n] [priority-for-child-0]
//   schedviz 4          -- 4 children, all default priority (31)
//   schedviz 4 5        -- 4 children, child 0 boosted to priority 5
//
// Place in xv6-riscv/user/, add $U/_schedviz\ to UPROGS.

#include "kernel/types.h"
#include "user/user.h"

#define MAXCHILD 8
#define DURATION 50   // ticks, ~0.5s at xv6's default tick rate
#define BARWIDTH 40

int
main(int argc, char *argv[])
{
  int n = 4;
  int boost_prio = -1;   // -1 means "don't touch child 0's priority"
  int pipefds[MAXCHILD][2];
  int children[MAXCHILD];
  uint64 counts[MAXCHILD];
  uint64 maxcount;
  int start, deadline;
  int i, j, pid, bars;

  if (argc > 1) {
    n = atoi(argv[1]);
  }
  if (n > MAXCHILD) {
    n = MAXCHILD;
  }
  if (n < 1) {
    n = 1;
  }
  if (argc > 2) {
    boost_prio = atoi(argv[2]);
  }

  start = uptime();
  deadline = start + DURATION;

  printf("Running %d children for %d ticks...\n", n, DURATION);
  if (boost_prio >= 0) {
    printf("(Child 0 gets priority %d, others stay at default 31)\n", boost_prio);
  }

  for (i = 0; i < n; i++) {
    if (pipe(pipefds[i]) < 0) {
      printf("schedviz: pipe failed\n");
      exit(1);
    }

    pid = fork();
    if (pid < 0) {
      printf("schedviz: fork failed\n");
      exit(1);
    }

    if (pid == 0) {
      close(pipefds[i][0]);
      volatile uint64 count = 0;
      while (uptime() < deadline) {
        count++;
        __sync_synchronize();
      }
      write(pipefds[i][1], (char *)&count, sizeof(count));
      close(pipefds[i][1]);
      exit(0);
    }

    children[i] = pid;
    close(pipefds[i][1]);

    if (i == 0 && boost_prio >= 0) {
      set_priority(pid, boost_prio);
    }
  }

  maxcount = 0;
  for (i = 0; i < n; i++) {
    read(pipefds[i][0], (char *)&counts[i], sizeof(counts[i]));
    close(pipefds[i][0]);
    wait(0);
    if (counts[i] > maxcount) {
      maxcount = counts[i];
    }
  }

  for (i = 0; i < n; i++) {
    printf("Child %d: %lu iterations  ", i, counts[i]);
    bars = (maxcount > 0) ? (int)(counts[i] * BARWIDTH / maxcount) : 0;
    for (j = 0; j < bars; j++) {
      printf("#");
    }
    printf("\n");
  }

  exit(0);
}
