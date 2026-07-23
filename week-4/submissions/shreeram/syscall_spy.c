// syscall_spy.c — Week 4, Problem 4: Syscall Spy
//
// Calls a handful of xv6 system calls and prints what each returns:
// getpid(), uptime(), fork(), write(), sleep(), getpid() again, dup(),
// wait(). Confirms that fork()'s return value in the parent matches
// the PID the child reports for itself, and that wait() reaps that
// same PID.
//
// Place in xv6-riscv/user/, add $U/_syscall_spy\ to UPROGS, rebuild:
//   $ syscall_spy

#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  int my_pid = getpid();
  int child_pid;
  int slept, written, dupfd, reaped;

  printf("Syscall Spy Report for PID %d:\n", my_pid);
  printf("  getpid()       -> %d\n", my_pid);
  printf("  uptime()       -> %d\n", uptime());

  child_pid = fork();
  if (child_pid < 0) {
    printf("syscall_spy: fork failed\n");
    exit(1);
  }

  if (child_pid == 0) {
    // fork() returned 0 -- we are the child.
    printf("I am the child!\n");
    printf("  --- in child (PID %d) ---\n", getpid());
    printf("  getpid()       -> %d\n", getpid());
    exit(0);
  }

  // Parent branch: fork() returned the child's PID.
  printf("  fork()         -> %d  (child PID)\n", child_pid);
  printf("  --- back in parent (PID %d) ---\n", my_pid);

  slept = sleep(10);
  printf("  sleep(10)      -> %d\n", slept);

  written = write(1, "Hello, world!\n", 14);
  printf("  write(1, ...)  -> %d\n", written);

  dupfd = dup(0);
  printf("  dup(0)         -> %d\n", dupfd);

  reaped = wait(0);
  printf("  wait()         -> %d  (reaped child PID)\n", reaped);

  if (reaped == child_pid) {
    printf("Match confirmed: fork() returned %d and wait() reaped %d.\n",
           child_pid, reaped);
  } else {
    printf("Mismatch: fork() returned %d but wait() reaped %d.\n",
           child_pid, reaped);
  }

  exit(0);
}
