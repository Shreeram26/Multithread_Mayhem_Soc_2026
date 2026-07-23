// getsyscount_test.c — Week 5, Problem 2: Syscall counter test
//
// Calls a handful of known syscalls, then calls getsyscount() and
// prints the result. Run it more than once to confirm the count
// increases by exactly the number of calls added in between.
//
// GOTCHA worth documenting (see REPORT.md): printf() itself makes a
// write() syscall internally. If you sandwich a printf() between
// two measurements you care about, the printf() call inflates the
// second one. This test is deliberately written to take BOTH
// getsyscount() readings first, with nothing but plain (non-printf)
// syscalls in between, and only prints afterward.
//
// Place in xv6-riscv/user/, add $U/_getsyscount_test\ to UPROGS.

#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  int n1, n2;

  getpid();
  uptime();
  sleep(0);
  n1 = getsyscount();          // does not count itself -- see sysproc.c

  getpid();
  getpid();
  getpid();
  n2 = getsyscount();          // again, does not count itself

  // Only now do we start using printf(), so its own write() calls
  // can't distort the n1/n2 readings above.
  printf("First reading  (after getpid+uptime+sleep(0)): %d\n", n1);
  printf("Second reading (after 3 more getpid() calls):  %d\n", n2);
  printf("Difference: %d (expect exactly 3)\n", n2 - n1);

  if (n2 - n1 == 3) {
    printf("PASS: count increased by exactly the number of calls added.\n");
  } else {
    printf("FAIL: expected a difference of 3.\n");
  }

  exit(0);
}
