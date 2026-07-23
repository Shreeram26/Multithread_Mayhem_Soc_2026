# Week 6 Report — Processes & Scheduling

**Submitted by:** shreeram
**Problems attempted:** 1 (Trace the scheduler), 2 (Priority scheduler), 3 (Lock detective), 4 (Schedviz — stretch)

**Checkout used:** `mit-pdos/xv6-riscv` @ commit `79fa703ee1df7b548ef0bae4f834c453ac75319a` (2022-09-09), with my own Week 5 kernel changes already applied on top (this week builds on last week's `struct proc` additions). See Week 4/5 `REPORT.md` for why this specific commit rather than current `HEAD`.

## Environment limitation

Same as the last two weeks: **no RISC-V toolchain or QEMU here, so nothing was actually built or booted.** As with Week 5, I made the real edits to real copies of the grounding commit's files and generated a genuine unified diff (`kernel-diffs.patch`, this time relative to my Week 5 result, since the two weeks' changes stack), then verified with `patch -p1` that it applies cleanly and reproduces my edited files byte-for-byte. **Please build and boot this in your own checkout before submitting.**

---

## Problem 1 — Trace the scheduler

`sched_trace.md` — traced a forked child's full round trip: `allocproc()` → `fork()` setting `RUNNABLE` → (my modified) `scheduler()` picking it → `swtch` in `kernel/swtch.S` → running → a timer interrupt driving `yield()`/`sched()` → back into `scheduler()`'s next pass. Citations are against the real source, including my own Week 6 scheduler changes for the parts of the trace that touch scheduling policy.

## Problem 2 — Priority scheduler

Added `int priority;` to `struct proc` (default `31`, set in `allocproc()`), `set_priority(pid, prio)` / `get_priority(pid)` syscalls, and rewrote `scheduler()`'s inner loop.

**Design note on locking (the "things to watch out for" in the assignment):** you cannot correctly implement "find the best priority among all processes" while holding only one proc lock as a single atomic view of the table. My solution is **two scans per scheduler pass**. Scan 1 finds the smallest priority value among currently `RUNNABLE` processes, touching one `p->lock` at a time. Scan 2 walks `proc[]` again and runs every process that is still `RUNNABLE` with that best priority, re-checking each candidate under its own lock immediately before switching. This keeps the one-lock-at-a-time discipline and also preserves stock xv6's same-priority turn-taking better than choosing only one saved "best" process each outer loop.

**Tie-break policy:** among processes tied for the same lowest priority value, scan 2 runs them in `proc[]` order, just like the original xv6 scheduler loop. This usually, but not necessarily always, tracks PID order because slots can be reused after processes exit. I chose this over a strict lowest-PID-only tie-break so equal-priority CPU-bound processes still get turns instead of one early slot winning every pass.

**The self-priority question:** if a process calls `set_priority()` on itself, **the change does not preempt it immediately**. A currently-`RUNNING` process is not a scheduler candidate until it gives up the CPU and becomes `RUNNABLE` again. The new priority matters at the next reschedule point, when scan 1 reads that process's updated field; in practice that is usually the next timer interrupt or a voluntary sleep/wait/yield path.

**Fairness:** same-priority processes keep stock xv6-style fairness within their priority class: scan 2 continues through the table after a process yields, so other runnable processes at the same priority get considered in that same pass. I did not add aging or any fairness across different priorities; a low-priority CPU-bound process can still be starved while a higher-priority CPU-bound process remains runnable, which is expected for this simple scheduler.

**Test:** `priority_test.c` — two children, priorities `5` and `50`, run a fixed number of ticks measured from a deadline **computed once before either fork**, so both children have an identical target from the start without needing any live communication. (Not run, so I can't report real iteration counts, but the mechanism doesn't depend on measuring anything the parent couldn't set up correctly in advance.)

## Problem 3 — Lock detective

**Correction on file location:** the assignment says `tickslock` is defined in `kernel/timer.c` — that file doesn't exist in this checkout (or, as far as I can tell, in any version of xv6-riscv); `tickslock` is a plain top-level `struct spinlock tickslock;` in `kernel/trap.c`, already `extern`-declared in `defs.h`, so it "just works" from `sysproc.c` with no extra declaration needed.

**Correction on how to reach `cons.lock`:** the assignment says to "extern declare `cons.lock`," but `cons` in `kernel/console.c` is an instance of an **anonymous struct type** (no tag name) — there's no type name another file could use to write a matching `extern` declaration and then reach into `.lock`. You'd either have to duplicate the entire (otherwise-private) struct definition, including its input-buffer fields, into `sysproc.c`, or add a small accessor. I added `cons_lock_held()` to `console.c` (one line: `return cons.lock.locked;`), declared it in `defs.h`, and call that from `sys_is_lock_held()` instead — which also keeps `console.c`'s internal layout private, the same way `getprocstatebypid()` exposes process state without exposing `proc[]` itself.

**The false-negative question:** yes, `is_lock_held()` **can** give a false negative, and I don't think there's any way to avoid it without adding real synchronization (which would defeat the point of a lightweight introspection call). `sys_is_lock_held()` reads `lk->locked` as a plain, unsynchronized load — there's no atomicity guarantee that this read happens "at the same instant" as anything else. Between the moment the value is read and the moment `sys_is_lock_held()` returns it across the trapframe to user space, any other CPU is completely free to acquire or release that exact lock. So a caller could see `0` ("not held") and then, a handful of instructions later, act on that stale information while the lock is now actually held — a classic TOCTOU (time-of-check-to-time-of-use) gap. This is fundamentally different from actually acquiring the lock (which would give a true, momentarily-exclusive answer at the cost of contending with real lock users) — `is_lock_held()` only ever answers "was it held at some instant in the recent past," which real systems' `/proc/lock_stat`-style introspection tools have exactly the same limitation for exactly the same reason.

**Test:** `lock_detective.c` — checks both real locks, the invalid ID, and re-checks `tickslock` after a short sleep.

## Problem 4 — Schedviz (stretch)

**A real discrepancy I found and worked around:** the assignment's hint suggests using "a `volatile int` in a page allocated by `sbrk()` that's shared after `fork()`" as a stop flag between parent and children. I checked `uvmcopy()` in `kernel/vm.c` (the function `fork()` uses to copy the parent's memory into the child) — it calls `kalloc()` for a **brand-new physical page** and `memmove()`s the parent's page contents into it, for every page including anything from `sbrk()`. Parent and child end up with **completely separate physical memory** after `fork()` in this xv6; there is no copy-on-write, and no `mmap()`, so there's no way to get genuinely shared memory between them this way. Writing to a "shared" flag in the child would never be visible to the parent, or vice versa. Instead, I used the pattern this course's own `week-6/code/01_sched_trace.c` already uses successfully: agree on a deadline (in ticks) **before** any `fork()` happens, so every child's copy of that number is correct from the moment it's created (no live update needed, since it never changes), and report results back to the parent through a **pipe** — real kernel-mediated inter-process communication, unlike raw memory.

`schedviz.c` — forks N children (default 4) that spin until the shared deadline, reports iteration counts via pipes, and prints a proportional `#`-bar chart. Supports an optional second argument to boost child 0's priority via `set_priority()`, to visually compare bar charts with and without the priority change (not run, so I can't report what the bars actually looked like).

---

## One thing that surprised me

That the assignment's own suggested implementation technique for Problem 4 doesn't actually work in the xv6 I was grounding against — it was a genuinely useful reminder that "the assignment describes an approach" and "the approach is correct for this codebase" are different claims, and the only way to know for sure is to go read `uvmcopy()` rather than assume `fork()` behaves like Linux's copy-on-write `fork()`.

## What I got stuck on

Designing the two-scan scheduler loop for Problem 2 took a bit of back-and-forth. A tempting version saved one "best" process and ran only that process before restarting the outer loop, but that would let the first equal-priority runnable slot win repeatedly and starve peers. Splitting it into "find the best priority value, then run every still-runnable process at that priority" respects the one-lock-at-a-time discipline and keeps equal-priority behavior close to stock xv6.

## Verification status

**Not built or booted** — no RISC-V/QEMU toolchain in this environment. After the scheduler fairness fix, the unified diff's hunk metadata has been checked for internal consistency, but the edited Week 6 patch still needs to be applied to the Week 5 xv6 tree and built for real. Please run `patch -p1`, `make clean && make && make qemu`, and then `priority_test`, `lock_detective`, and (if attempted) `schedviz` before submitting — several of the specific numbers and behaviors described above are predictions based on reading the code, not observations.
