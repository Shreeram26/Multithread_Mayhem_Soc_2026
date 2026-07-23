# Week 4 Report — xv6 Setup & Codebase Exploration

**Submitted by:** shreeram
**Problems attempted:** 1 (Hello xv6!), 2 (Codebase Detective), 3 (Fork Chain), 4 (Syscall Spy)

## Important: an environment limitation, and how I worked around it

I do not have a RISC-V cross-compiler or QEMU available in this environment, so **I could not actually clone, build, or boot xv6-riscv, and none of this week's code has been run inside a real xv6 shell.** Everything below was written by reading the real xv6-riscv source directly (fetched from GitHub) rather than from memory, cross-checked line-by-line against the exact function signatures, file layouts, and behavior the code needs to compile and run correctly against — but "should be correct by careful reading" is not the same as "verified by running." **Before submitting, please actually build this in your own checkout (`make clean && make && make qemu`) and confirm the output matches what's described here.**

### A version mismatch worth knowing about

While grounding this week's work I discovered something that will bite anyone in this course who runs `git clone https://github.com/mit-pdos/xv6-riscv.git` today, exactly as the `README.md` instructs: **the current `HEAD` of the repo's `riscv` branch has been substantially refactored** since this course's own reference material (`week-5/code`, `week-6/code`) was written:

- `sleep()` (a syscall used directly in this course's `01_arg_passing_demo.c`) has been renamed `pause()`.
- `sys_fork()` / `sys_exit()` / `sys_wait()` / `sys_kill()` now call wrapper functions (`kfork()`, `kexit()`, `kwait()`, `kkill()`) instead of implementing the logic directly — the course's own worked examples (`00_worked_example_hello_syscall.md`, `02_worked_example_sched_print.md`) assume the older, direct style.
- Two new syscalls (`SYS_pause`, `SYS_sync`) exist, and `sbrk` gained a lazy-allocation mode with a second argument — so **"count the syscalls in `kernel/syscall.h`" gives a different number** depending on which commit you clone.

I traced the commit history and found the last commit before this refactor: **`79fa703ee1df7b548ef0bae4f834c453ac75319a`, dated 2022-09-09.** Its API matches this course's worked examples exactly (`sleep()`, direct `sys_fork()`, classic `scheduler()` loop, `SYS_close = 21` as the highest syscall number). **I wrote all of Week 4/5/6's code against that commit.** If your actual assigned checkout is a plain `git clone` of `HEAD` today, quite a bit of this (syscall numbers, the `sleep`/`pause` name, the `kexit`/`kfork` wrapper layer) will not line up, and you should either checkout that specific commit (`git checkout 79fa703`) or ask your mentor which commit/tag the course actually expects. This felt important enough to flag loudly rather than silently write code that would confuse you when it didn't match your own build.

---

## Problem 1 — Hello xv6!

`hello_xv6.c` — prints a greeting, `argc - 1` as the argument count, each argument on its own line, and the syscall count. I counted **21** syscalls directly from `kernel/syscall.h` at the commit above (`SYS_fork` through `SYS_close`) — not the `23` shown as an illustrative example in the problem statement's expected output, which appears to just be a placeholder number for a hypothetical build; whatever your own `kernel/syscall.h` actually contains is the number that belongs in `NUM_SYSCALLS`, which is the entire point of the exercise.

## Problem 2 — Codebase Detective

`codebase-detective.md` — full answers to A–D, each citing real file names and line numbers from the grounding commit. One thing worth flagging again here: the "five states" in the problem statement refers to the five states an *existing* process can be in (`USED`, `SLEEPING`, `RUNNABLE`, `RUNNING`, `ZOMBIE`) — `kernel/proc.h`'s `enum procstate` actually has six values, with `UNUSED` marking an empty `proc[]` slot that isn't really "a process" at all.

## Problem 3 — Fork Chain

`fork_chain.c`. The one real discovery here: **stock xv6 has no `getppid()` syscall** — I checked `user/user.h` directly and it isn't there, only `getpid()`. The expected sample output in the problem statement shows a real parent PID for every depth including depth 0, which would need `getppid()` to get for free. Since Problem 4's own instructions are explicit that "No kernel modifications required" this week, I worked around it the only way available in pure user space: capture `my_pid = getpid()` *before* calling `fork()`, and pass it down as an ordinary function argument — `fork()` copies the whole address space, so the child inherits that already-correct value without needing any new syscall. This works for every depth ≥ 1. The root process (depth 0) genuinely has no way to learn the shell's PID without a real `getppid()`, so it prints `N/A` there rather than fabricating a number — an honest limitation, not a bug.

By construction, "total processes in chain" and "depth reached" always agree exactly (`total == n`, `depth reached == n - 1`) — this is a straight chain, not a tree, so there's no runtime scenario where they'd disagree; the "verify they match" instruction is really checking that you understand *why* they must match, not running a check that could fail.

## Problem 4 — Syscall Spy

`syscall_spy.c` — calls `getpid()`, `uptime()`, `fork()`, and (in the child) `getpid()` again with an explicit `"I am the child!"` marker on the `pid == 0` branch; in the parent, `sleep(10)`, `write(1, ...)`, `dup(0)`, and `wait(0)`, with an explicit comparison confirming `fork()`'s returned child PID matches the PID `wait()` reaps.

---

## What surprised me

That the "textbook" version of xv6-riscv that virtually every OS course (including this one) is built around has quietly diverged from what `git clone`-ing the canonical repo today actually gives you. I wouldn't have caught this without actually diffing the current `HEAD` against the commit history rather than trusting general knowledge of "what xv6 looks like."

## What I got stuck on

Realizing `getppid()` doesn't exist in xv6 took a moment — my first draft of `fork_chain.c` called it directly, and it would have failed to link (`usys.pl` has no entry for it, so there's no user-space stub). Re-reading `user/user.h` line by line caught it before I'd have wasted a build cycle on it (which is exactly the value of "read the source" that Problem 2 is trying to teach).

## Verification status

**Not run inside xv6/QEMU** — no RISC-V toolchain available in this environment. Written by careful reading of the actual grounding commit's source rather than from memory; please build and test in your own checkout before submitting, and treat the depth/PID numbers in any sample output here as illustrative, not verified console output.
