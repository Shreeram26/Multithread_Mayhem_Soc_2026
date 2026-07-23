// getcmdline_test.c — Week 5, Problem 4: Safe copy-out test
//
// Calls getcmdline() once with a generously-sized buffer (should
// print back this program's own name in full), and once with a
// tiny buffer (should print the truncated result), to demonstrate
// both the normal path and the truncation path.
//
// Place in xv6-riscv/user/, add $U/_getcmdline_test\ to UPROGS.

#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  char buf[64];
  char tiny[4];
  int n;

  n = getcmdline(buf, sizeof(buf));
  printf("getcmdline(buf, 64)  -> %d, buf = \"%s\"\n", n, buf);

  n = getcmdline(tiny, sizeof(tiny));
  printf("getcmdline(buf, 4)   -> %d, buf = \"%s\" (truncated)\n", n, tiny);

  n = getcmdline(buf, 0);
  printf("getcmdline(buf, 0)   -> %d (expect -1, max must be > 0)\n", n);

  exit(0);
}
