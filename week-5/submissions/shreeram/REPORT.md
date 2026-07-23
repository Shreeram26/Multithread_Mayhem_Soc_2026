# Week 5 Report — System Calls

**Submitted by:** shreeram
**Problems attempted:** 1 (Trace a syscall by hand), 2 (Syscall counter), 3 (Process peeker), 4 (Safe copy-out)

**Checkout used:** `mit-pdos/xv6-riscv` @ commit `79fa703ee1df7b548ef0bae4f834c453ac75319a` (2022-09-09) — see Week 4's `REPORT.md` for the full explanation. Short version: the current `HEAD` of the repo has been refactored (`sleep()`→`pause()`, direct `sys_fork`/`sys_exit` → `kfork`/`kexit` wrappers) in ways that no longer match this course's own worked examples, so I traced back to the last commit that does match and built everything against that.

## Environment limitation

Same as Week 4: **no RISC-V toolchain or QEMU available here, so nothing below was actually built or booted.** What I *could* do, and did: fetch the real source for the grounding commit, make the actual edits to real copies of `kernel/proc.h`, `kernel/proc.c`, `kernel/defs.h`, `kernel/syscall.h`, `kernel/syscall.c`, `kernel/sysproc.c`, `user/user.h`, and `user/usys.pl`, and generate a real unified diff (`kernel-diffs.patch`) from the actual before/after file contents. I then verified that patch applies cleanly with `patch -p1` to a fresh copy of the grounding commit and produces byte-identical output to my working copy — so the *diff itself* is mechanically verified to be well-formed and internally consistent, even though I couldn't compile or boot the result. **Please run `make clean && make && make qemu` in your own checkout and confirm before submitting** — I'm confident in the logic but this is not the same as watching it boot.

---

## Problem 1 — Trace a syscall by hand

`syscall_trace.md` — traced `getpid()` end-to-end through the real source of the grounding commit, citing actual file/line numbers throughout. One thing worth flagging: this specific commit doesn't have a separate `usertrapret()` function the way some course material describes — the "prepare for return" logic is a helper called `prepare_return()`, invoked from inside `usertrap()` itself right before it returns. If your `theory.md` describes a standalone `usertrapret()`, that's a slightly different revision of xv6 than the one I traced; the *behavior* (set up `sstatus`/`sepc`, then fall through into the trampoline's `userret` for the final `sret`) is the same regardless of which function it's packaged in.

## Problem 2 — Syscall counter

Added `int syscalls;` to `struct proc`, initialized to `0` in `allocproc()`, incremented unconditionally at the top of `syscall()` in `kernel/syscall.c` (before the valid/invalid check, so it counts *every* call attempt, exactly as instructed) — see `kernel-diffs.patch`.

**The "does it count itself" question:** I chose to make `getsyscount()` **not** count itself. Since the increment happens inside `syscall()` before the handler runs, by the time `sys_getsyscount()` executes, `p->syscalls` has already been bumped for this very call — so `sys_getsyscount()` returns `myproc()->syscalls - 1` instead of the raw field. My reasoning: the natural way a caller will use this is "how many syscalls did my program make *up to this point in my own code*" — e.g. exactly the pattern in `getsyscount_test.c`, comparing two readings to check a delta. If the call counted itself, every single reading would be inflated by exactly 1 relative to "syscalls before I asked," which is a constant, uninteresting offset that would need to be manually subtracted by every caller anyway. Making the kernel do that subtraction once, in one place, felt more useful than reproducing "off by one, but a *documented* off by one" in every caller. There's a legitimate opposite view too (the query itself genuinely *is* a syscall, so reporting the raw uncorrected count is more "honest" about what actually happened) — I don't think there's a uniquely correct answer here, just a design choice, and I've stated mine and why.

**Test:** `getsyscount_test.c`. One subtlety I discovered while writing it: `printf()` itself makes a `write()` syscall internally, so sandwiching a `printf()` between two `getsyscount()` readings you're trying to diff will silently inflate the second reading by however many syscalls that `printf()` made. I restructured the test to take both readings first, with only plain (non-`printf`) syscalls in between, and print everything afterward — worth calling out since it's an easy trap to fall into.

## Problem 3 — Process peeker

Added `getprocstatebypid()` in `kernel/proc.c`, modeled directly on `kill()` — same "acquire one `p->lock` at a time while scanning the whole `proc[]` array, never hold two at once" discipline, for exactly the reason the assignment calls out: holding a second process's lock while already holding another is how deadlocks get built.

**Test:** `procpeek_test.c`. Forks a child that spins in a do-nothing loop, has the parent `sleep(2)` to give the child a real chance to be scheduled, then calls `getprocstate(childpid)`. I expect (not run, so genuinely a prediction rather than an observation) this will most often report `RUNNABLE` or `RUNNING`, and occasionally `SLEEPING` or `ZOMBIE` if the child finishes its loop and exits before the parent's `sleep(2)` returns — worth actually confirming once you can boot this. The nonexistent-pid check (`getprocstate(9999)`) is expected to reliably return `-1`, since `nextpid` starts at `1` and would need thousands of prior `fork()`s since boot to ever collide with `9999` in a fresh QEMU session.

## Problem 4 — Safe copy-out

`sys_getcmdline()` reads the user's buffer pointer and length via `argaddr`/`argint`, validates `max > 0`, computes the safe copy length (`min(strlen(p->name), max - 1)`), and uses two `copyout()` calls (name bytes, then a single NUL byte) rather than ever touching `buf` through a raw pointer.

**Why writing through `buf` directly would be wrong:** `p->name` is kernel memory, and the `buf` *value* the user program passed is a **user virtual address** — valid only when interpreted through that process's own page table. The kernel is running with its own page table active at the time `sys_getcmdline()` executes (see the syscall trace above: `uservec` switches to the kernel page table before ever calling into C code, and doesn't switch back until `userret`). If the kernel dereferenced `buf` directly as a plain `char *`, the CPU would resolve that virtual address through the *kernel's* page table, not the user's — at best hitting an unrelated (or unmapped) physical page and either corrupting unrelated kernel state or immediately faulting into a `panic`. `copyout()` is exactly the function that knows how to walk a *specific* page table (`p->pagetable`, passed explicitly) to translate that same virtual address correctly, which is the entire reason `argaddr()` hands back a `uint64` instead of a real, dereferenceable pointer.

**Test:** `getcmdline_test.c` — exercises the normal path (buffer of 64), the truncation path (buffer of 4), and the `max <= 0` rejection path.

---

## One thing that surprised me

How much of "the hardest idea this week" (per the assignment) really does come down to one sentence once you trace it concretely: a user-space pointer is only meaningful under a *specific* page table, and the kernel is running under a different one at the exact moment it would need to dereference it. Writing the `sys_getcmdline` reasoning above made that concrete in a way that just reading about `copyin`/`copyout` abstractly hadn't.

## What I got stuck on

Getting the `getsyscount_test.c` measurement window right (the `printf`-makes-a-syscall gotcha above) — my first draft printed a reading immediately after taking it, which silently broke the very delta I was trying to verify.

## Verification status

**Not built or booted** — no RISC-V/QEMU toolchain in this environment. The kernel diff (`kernel-diffs.patch`) is mechanically verified to apply cleanly via `patch -p1` against the grounding commit and reproduce my edited files byte-for-byte; the C logic itself has been read and re-read carefully but not compiled or run. Please `make clean && make && make qemu` in your own checkout before submitting.
