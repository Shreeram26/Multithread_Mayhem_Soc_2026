// hello_xv6.c — Week 4, Problem 1: Hello xv6! (Warm-up)
//
// Place in xv6-riscv/user/, add $U/_hello_xv6\ to UPROGS in the
// xv6-riscv Makefile, then:
//   make clean && make && make qemu
//   $ hello_xv6
//   $ hello_xv6 hi there!
//
// NOTE ON NUM_SYSCALLS: counted by hand from kernel/syscall.h in
// mit-pdos/xv6-riscv @ commit 79fa703 (2022-09-09) -- SYS_fork(1)
// through SYS_close(21), 21 entries total. This is the last commit
// whose API matches this course's own worked examples (sleep(),
// direct sys_fork()/sys_exit(), no kexit/kfork wrappers) -- see
// REPORT.md for why the current HEAD of the repo no longer matches.
// If your own checkout's kernel/syscall.h has a different highest
// SYS_* number, update NUM_SYSCALLS to match -- that's the whole
// point of the exercise ("you'll need to read the file to count
// them").

#include "kernel/types.h"
#include "user/user.h"

#define NUM_SYSCALLS 21

int
main(int argc, char *argv[])
{
  int i;

  printf("Hello, xv6! This is Shreeram.\n");
  printf("Number of arguments: %d\n", argc - 1);

  for (i = 1; i < argc; i++) {
    printf("arg %d: %s\n", i, argv[i]);
  }

  printf("xv6 knows %d system calls.\n", NUM_SYSCALLS);

  exit(0);
}
