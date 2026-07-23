// fork_chain.c — Week 4, Problem 3: Fork Chain
//
// Builds a chain of N processes: process 0 (depth 0) forks a child
// (depth 1), which forks its own child (depth 2), and so on until
// depth N-1, the leaf. Each process prints its own PID, its parent's
// PID, and its depth, then waits for its single child before exiting
// (so the chain's exits unwind in reverse depth order).
//
// NOTE on parent PID: stock xv6 (checked against kernel/user.h in
// mit-pdos/xv6-riscv @ 79fa703) has NO getppid() syscall -- only
// getpid(). So "parent PID" is obtained the only way available
// without touching the kernel: capture my_pid = getpid() *before*
// calling fork(), and pass it down as an ordinary function argument.
// fork() copies the whole address space, so the child inherits that
// already-correct value for free. This works for every depth >= 1.
// The root process (depth 0) has no parent PID to report this way --
// it would need a real getppid() syscall (out of scope this week,
// see REPORT.md) -- so it honestly prints "N/A" instead of a made-up
// number.
//
// Only the leaf has no child to fork; it prints the "I am the leaf!"
// message and the twist: the total process count and the depth
// reached. By construction these always agree (total == N,
// depth reached == N-1), since the chain is a straight line, not a
// tree -- see REPORT.md for why that's worth calling out rather than
// treating as a runtime check that could fail.
//
// Place in xv6-riscv/user/, add $U/_fork_chain\ to UPROGS, rebuild:
//   $ fork_chain 3

#include "kernel/types.h"
#include "user/user.h"

void
chain(int depth, int n, int parent_pid, int have_parent)
{
  int my_pid = getpid();
  int pid;

  if (have_parent) {
    printf("Depth %d: PID %d, Parent PID %d\n", depth, my_pid, parent_pid);
  } else {
    printf("Depth %d: PID %d, Parent PID N/A (xv6 has no getppid())\n",
           depth, my_pid);
  }

  if (depth == n - 1) {
    printf("I am the leaf! No more children.\n");
    printf("Total processes in chain: %d\n", n);
    printf("Depth reached: %d\n", depth);
    exit(0);
  }

  pid = fork();
  if (pid < 0) {
    printf("fork_chain: fork failed at depth %d\n", depth);
    exit(1);
  }

  if (pid == 0) {
    // Child continues the chain one level deeper, carrying this
    // process's own (correct) pid down as the child's parent_pid.
    // chain() always ends in exit(), so control never returns here.
    chain(depth + 1, n, my_pid, 1);
  } else {
    wait(0);
    exit(0);
  }
}

int
main(int argc, char *argv[])
{
  int n = 5;

  if (argc > 1) {
    n = atoi(argv[1]);
  }
  if (n < 1) {
    n = 1;
  }

  chain(0, n, 0, 0);  // depth 0 has no known parent pid -- see note above

  exit(0);  // unreachable -- chain() never returns
}
