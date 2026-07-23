# Week 4, Problem 2 — Codebase Detective

**Checkout used:** `mit-pdos/xv6-riscv` @ commit `79fa703ee1df7b548ef0bae4f834c453ac75319a` (2022-09-09). See `REPORT.md` for why I deliberately did not use the current `HEAD` of the `riscv` branch — it has since been refactored in ways that no longer match this course's own example code (`sleep()` renamed to `pause()`, `sys_fork`/`sys_exit` now call wrapper functions `kfork()`/`kexit()`, etc.). Line numbers below are approximate, as always with "read the source" exercises — they're accurate for this exact commit and may drift by a line or two in yours.

---

## A. Process states (`kernel/proc.h`)

**Line 82:** `enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };`

That's six enum values, not five — but `UNUSED` isn't really a *state a process is in*, it's the marker for a `proc[]` slot that currently holds no process at all (checked by `allocproc()` when looking for a free slot to reuse). The **five states an actual, existing process moves through** are:

1. **USED** — just claimed by `allocproc()`, not yet ready to run
2. **RUNNABLE** — ready to run, waiting for the scheduler to pick it
3. **RUNNING** — currently executing on a CPU
4. **SLEEPING** — blocked, waiting on some event (a "channel," `p->chan`)
5. **ZOMBIE** — exited, waiting for its parent to `wait()` it and reclaim its slot

**Right after `fork()` returns in the child:** this needs two answers depending on which "returns" you mean. The *parent's* call to `fork()` (in `kernel/proc.c`, the C function `fork()`, around **line 296**) sets `np->state = RUNNABLE` before returning the new PID to the parent — so immediately after creation, the child sits in `RUNNABLE`. But the *child's own code* only "sees" `fork()` return (getting `0` back, set at **line 277**: `np->trapframe->a0 = 0;`) once the scheduler has actually picked it up and context-switched to it — and `scheduler()` sets `p->state = RUNNING` (**line 441**) immediately before that `swtch`. So: the slot transitions through `RUNNABLE` first, but by the time the child process itself is executing the instruction after its `fork()` call, its own state is `RUNNING`.

**Between `exit()` and `wait()`:** `ZOMBIE`. `exit()` in `kernel/proc.c` sets `p->state = ZOMBIE` (**line 353**) right before parking itself in the scheduler forever via `sched()`; the slot stays `ZOMBIE` — pid, exit status (`xstate`), and all — until the parent's `wait()` finds it and calls `freeproc()`, which resets it to `UNUSED` (`kernel/proc.c`, `freeproc()`, **line 171**).

---

## B. The boot process (`kernel/main.c`)

**`main()`, lines 10–45.** On CPU 0, in order:

1. `consoleinit()` — console (UART) driver
2. `printfinit()` — kernel `printf`
3. `kinit()` — physical page allocator
4. `kvminit()` — build the kernel's page table
5. `kvminithart()` — turn on paging for this hart
6. `procinit()` — initialize the `proc[]` table
7. `trapinit()` — trap-related locks (`tickslock`)
8. `trapinithart()` — install the kernel trap vector
9. `plicinit()` / `plicinithart()` — interrupt controller
10. `binit()` — buffer cache
11. `iinit()` — inode table
12. `fileinit()` — open-file table
13. `virtio_disk_init()` — emulated disk
14. `userinit()` — create the very first process (`initproc`, runs `/init`)
15. set `started = 1` so the other harts can proceed past their spin-wait

Other CPUs (hart != 0) just wait for `started`, then run `kvminithart()` / `trapinithart()` / `plicinithart()` for themselves.

**First subsystem initialized:** `consoleinit()` — makes sense, since every single step after it wants to be able to print. **Last one (on CPU 0) before the first process exists:** `virtio_disk_init()`, immediately followed by `userinit()` which is really "create the first process," not a subsystem init.

**`scheduler()` (line 44 calls it; the function itself is in `kernel/proc.c`, lines 419–457):** loops forever. Each pass scans the whole `proc[]` array; for each proc, it acquires `p->lock`, and if `p->state == RUNNABLE`, sets it to `RUNNING`, records `c->proc = p`, and calls `swtch()` to context-switch into it. When that process later gives the CPU back (via `sched()`), control returns right after the `swtch` call, `c->proc` is cleared to 0, and the lock is released before moving to the next slot. If a whole pass finds no `RUNNABLE` process at all, the CPU executes `wfi` ("wait for interrupt") instead of busy-spinning.

---

## C. The system call table (`kernel/syscall.c`)

**How many syscalls does xv6 currently support?** **21**, `SYS_fork` (1) through `SYS_close` (21) — counted directly from `kernel/syscall.h` in this commit.

**The `syscalls[]` array (lines 107–129) and the index/number pattern:** it's built with C99 designated initializers, `[SYS_fork] sys_fork, [SYS_exit] sys_exit, ...` — so the **array index is literally the syscall number** from `syscall.h`. That's the whole trick behind dispatch: `syscall()` (line 131) reads the number out of the trapframe's `a7` register (**line 137**: `num = p->trapframe->a7;`) and uses it directly as an array index (**line 141**: `syscalls[num]()`), no separate lookup or `switch` statement needed. Since the numbers happen to be contiguous (1–21 with no gaps), every slot in the array is populated; if a course fork ever skips a number, that slot would just be an implicit null pointer.

**Invalid syscall number:** no handler function is called at all. The bounds check at **line 138** (`if(num > 0 && num < NELEM(syscalls) && syscalls[num])`) fails, so the `else` branch (**lines 142–146**) runs instead — it `printf`s `"%d %s: unknown sys call %d\n"` to the console and sets `p->trapframe->a0 = -1;` directly, so from the calling user program's point of view, the syscall wrapper just returns `-1`. There's no crash and no `panic` — an invalid syscall number is handled gracefully.

---

## D. The shell (`user/sh.c`)

**`runcmd()` (lines 58–132):** switches on `cmd->type`. For a plain `EXEC` command, it does **not** fork itself — it calls `exec(ecmd->argv[0], ecmd->argv)` directly (**line 79**) to replace the *current* process image with the requested program. The actual `fork()` happens one level up: in `main()`'s command loop (`if(fork1() == 0) runcmd(parsecmd(cmd)); wait(0);`, **lines 172–175**), or, for compound commands (`LIST`, `PIPE`, `BACK`), inside `runcmd()` itself via `fork1()` (a tiny wrapper around `fork()` that panics on failure — defined at **line 187**). So the shape is always: fork a child, and only inside that child does `runcmd()` (eventually) call `exec()`. If `exec()` fails, `runcmd()` prints `"exec %s failed\n"` (**line 80**) and falls through to `exit(0)` at the very end of the function (**line 131**) — every path through `runcmd()` ends in `exit()`, matching its declared `__attribute__((noreturn))` (**line 55**).

**`ls | cat`:** the parser builds a `struct pipecmd` (type `PIPE`) whose `left` is an `execcmd` for `ls` and `right` is an `execcmd` for `cat` — `parsepipe()` (**lines 367–378**) calls `parseexec()` for the left side, then, on seeing a `|` token, recurses to build the right side and wraps both in `pipecmd()`. At execution time, the `PIPE` case in `runcmd()` (**lines 101–123**) does the classic pipe dance: `pipe(p)` creates the two file descriptors; the first `fork1()`'d child closes its own fd 1, `dup(p[1])`s the pipe's write end onto fd 1, closes both raw pipe fds, then recursively calls `runcmd(pcmd->left)` (so `ls`'s stdout writes into the pipe); the second `fork1()`'d child does the mirror image on fd 0 with `p[0]` and then `runcmd(pcmd->right)` (so `cat` reads its stdin from the pipe); the shell's own child closes both raw pipe ends and `wait()`s for both grandchildren before returning. Tracing it confirms `ls`'s output really does flow to `cat`'s input purely through file descriptor plumbing — no special "pipe" logic exists in `ls.c` or `cat.c` themselves, they just read/write fd 0/1 as always.
