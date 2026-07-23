# Week 2 Report — Synchronization

**Submitted by:** shreeram
**Problems attempted:** 1 (Fix last week's mess), 2 (Thread-safe bounded buffer), 3 (Thread-safe linked list), 4 (Dining philosophers — stretch)

All programs compile cleanly with `gcc -Wall -pthread -std=c11` (or `-pthread` for the C99-style Week 1 fixes) with no warnings, and every program terminates normally when run.

---

## Problem 1 — Fix last week's mess

`submissions/shreeram/problem-1/parallel_sum_fixed.c` and `bank_chaos_fixed.c`

**Part A — `parallel_sum_fixed.c`.** Each thread computes its partial sum into a *local* variable first (no locking needed — nothing shared yet), and only the final `total += local` merge is inside `pthread_mutex_lock`/`unlock`. This keeps the critical section tiny: one lock/unlock per thread, not once per array element.

Ran 20 times: **correct all 20 times**, `10,000,000` every run, no variation at all.

**Part B — `bank_chaos_fixed.c`.** The check-then-act (`if (balance >= 1) balance -= 1;`) is now entirely inside one `lock`/`unlock` pair, so no other thread can act on `balance` between the check and the subtraction.

Ran 20 times: **balance never went negative**, and `successful_alice + successful_bob` always accounted exactly for `10000 - final_balance` — the accounting is fully consistent every run, unlike Week 1's version where the outcome depended on luck.

**Reflection — timing racy vs. fixed `parallel_sum` (both built with `-O2` for a fair comparison, 3 runs each):**

| Version | Run 1 | Run 2 | Run 3 |
|---|---|---|---|
| Racy (Week 1) | 0.129s | 0.091s | 0.091s |
| Fixed (mutex) | 0.141s | 0.126s | 0.160s |

The mutex version is consistently slower (roughly 30–50% more wall-clock time here). The extra time isn't computing anything new — it's the cost of `pthread_mutex_lock`/`unlock` (an atomic operation plus, under contention, a possible trip through the kernel to block/wake a thread), and cache-line traffic: every thread that touches the shared `total_lock`/`total` pair invalidates the other cores' cached copies of that cache line, so even the "fast path" lock isn't free on a multi-core machine. Correctness has a real, measurable cost — which is exactly why the buffer/list implementations below try to keep critical sections as small as possible.

---

## Problem 2 — Thread-safe bounded buffer

`submissions/shreeram/problem-2/bounded_buffer.c`

Generic `buffer_init`/`buffer_put`/`buffer_get`/`buffer_destroy` API built directly on the `03_producer_consumer.c` pattern (one mutex + two condition variables, `while`-loop waits for spurious-wakeup safety), but with no knowledge of producers/consumers baked into the buffer itself.

**Test:** 3 producers (each producing 1000 known values, `N*1000..N*1000+999`) and 2 consumers sharing an 8-slot buffer (small on purpose, to force blocking on both ends). The last producer to finish pushes one "poison pill" per consumer so each consumer knows when to stop.

Result: `Produced: count=3000 sum=7498500` / `Consumed: count=3000 sum=7498500` — **exact match** on every run I tried.

Edge cases considered:
- **Capacity 1**: works — verified by re-running with `buffer_init(1)`; producer and consumer are then tightly lock-stepped, which is slower but still correct.
- **`signal` vs `broadcast`**: with a single waiter type per condition variable in this design (any waiting producer can make progress once one slot frees, any waiting consumer once one item arrives), `pthread_cond_signal` is sufficient and correctness doesn't change if I swap in `pthread_cond_broadcast` — it would just occasionally wake more threads than necessary, only one of which will actually find the condition still true (the rest loop back to `cond_wait` immediately). No performance difference was visible at this scale with only 2 consumers.

---

## Problem 3 — Thread-safe linked list

`submissions/shreeram/problem-3/safe_list.c`

Single coarse-grained `pthread_mutex_t` protecting the whole list, stored alongside `head` rather than inside each node (the pitfall called out in the assignment — a per-node lock wouldn't protect the head pointer or multi-node operations like insert/remove).

**Test:** 8 worker threads × 1000 mixed insert/contains/remove operations on values in `0–99`, using a small per-thread xorshift PRNG (swapped in for `rand_r`, which MinGW's UCRT headers don't declare under `-std=c11`) so threads frequently collide on the same values.

Ran 3 times: no crashes, list walked cleanly from head to `NULL` every time (lengths varied run to run — 539, 526, 520 — which is expected since insert/remove ordering is nondeterministic under concurrency), and the targeted check (`list_insert(999999)` then `list_contains(999999)` → `1`, `list_contains(-1)` → `0`) passed every time.

---

## Problem 4 — Dining philosophers (stretch)

`submissions/shreeram/problem-4/philosophers.c`

**Broken version (not submitted, built as a throwaway to observe the failure):** every philosopher locks their left fork, then their right. Output looked like:

```
Philosopher 3 picked up LEFT fork 3
Philosopher 0 picked up LEFT fork 0
Philosopher 4 picked up LEFT fork 4
Philosopher 2 picked up LEFT fork 2
Philosopher 1 picked up LEFT fork 1
```

...and then nothing else, ever — the process had to be killed after a timeout. All five threads grabbed their left fork essentially simultaneously; every philosopher's "right fork" is some other philosopher's "left fork," which is already held, so every thread blocks forever on its second `lock`. Classic circular wait.

**Fix — lock ordering.** Each philosopher computes `first = min(left, right)`, `second = max(left, right)` and always locks `first` before `second`, regardless of which side of the table it's physically on. This removes the cycle: fork 4 (the highest-numbered) is never the *first* lock any philosopher waits on, so there's always at least one philosopher able to make progress.

Ran the fixed version several times, each philosopher configured to eat at least 10 times: it always completes cleanly within a couple of seconds, e.g.:

```
Min meals: 10, Max meals: 10, Spread: 0
```

**Starvation check:** across multiple runs, meal counts came out perfectly even (10/10/10/10/10) every time at this scale (`EAT_TARGET=10`, short `usleep` delays). I did not see starvation, but I'd expect it to become visible only with a much longer run and more philosophers/contention — at this small scale the round-robin-ish OS scheduling combined with short critical sections doesn't give any one philosopher a systematic disadvantage.

---

## One thing that surprised me

How *fast* mutex contention costs show up even in single-digit-microsecond critical sections — the 30–50% slowdown in the `parallel_sum` reflection question felt bigger than I expected for "just" adding one lock/unlock pair per thread (not per element).

## What I got stuck on

None of the four problems required real debugging once I understood the corresponding Week 1 bug — the hardest part was making sure `safe_list.c` compiled at all under MinGW's `-std=c11` (no `rand_r`), which was a portability issue rather than a concurrency one.
