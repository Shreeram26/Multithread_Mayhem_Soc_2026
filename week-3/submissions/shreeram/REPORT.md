# Week 3 Report — Pthreads API & Classic Problems

**Submitted by:** shreeram
**Problems attempted:** 1 (Parallel statistics), 2 (Semaphore bounded buffer), 3 (Readers-writers from scratch), 4 (Thread pool)

All four programs compile cleanly with `gcc -Wall -Wextra -pthread -std=c11` with no warnings, and every program terminates normally.

---

## Problem 1 — Parallel statistics

`parallel_stats.c`

Computed **sum, min, max, and average** over a 10-million-element array filled with pseudo-random values in `[-1000, 1000]`. Used Approach A from `02_return_values.c` (results written into each thread's own argument struct — `partial_sum`, `partial_min`, `partial_max`, `partial_count`) rather than heap-allocating a return value, since there's nothing to free this way and no risk of returning a dangling stack pointer.

Sample run: combined sum `-144,405,064` over `10,000,000` elements → average `-14.4405`, min `-1000`, max `1000`. Re-ran several times — results are perfectly stable, because the array is seeded deterministically (`srand(1234)`) and each thread only ever touches its own chunk before the (lock-free, since each struct field is exclusively owned by one thread) merge step in `main`.

---

## Problem 2 — Semaphore bounded buffer

`sem_buffer.c`

Rebuilt the Week 2 bounded buffer with two counting semaphores (`empty` starting at `capacity`, `full` starting at `0`) instead of two condition variables; a mutex still protects the raw `in`/`out`/`data[]` indices, since multiple producers/consumers can still race on those even with the semaphores handling the wait logic.

**How it differs from the Week 2 condition-variable version:** the semaphore's internal count *is* the condition — `sem_wait` blocks exactly when the count is zero and unblocks the instant another thread posts, with no need to re-check anything after waking up. The condition-variable version needs an explicit `while (count == capacity) pthread_cond_wait(...)` loop because of spurious wakeups and because the condition can change again before you get the lock back; here there's no such loop, since a successful `sem_wait` already atomically reserved a "slot" (bookkeeping the semaphore does internally). It's a small amount of code, but the reasoning about correctness is noticeably simpler.

Tested with 3 producers / 2 consumers at two capacities: `1` (tightly lock-stepped) and `64` (mostly unblocked). Both trials: `Produced: count=3000 sum=7498500`, `Consumed: count=3000 sum=7498500` — exact match, `PASS` on both.

---

## Problem 3 — Readers-writers from scratch

`rw_lock.c`

Built from a plain mutex + two condition variables (not `pthread_rwlock_t`). **Policy: writer preference** — a new reader blocks if any writer is either active *or waiting*, even if the lock is technically free to read right now. I chose this deliberately over reader preference because reader preference can starve writers indefinitely under a steady stream of readers, which felt like the worse failure mode for a "database"-style shared resource; the tradeoff is that readers can starve under a constant stream of writers, which I've noted rather than hidden.

Stress test with 6 readers / 2 writers, run 5 times:
- **Max concurrent readers observed: 6** every run — confirms readers really do run in parallel, not serialized.
- **Writer/reader overlap detected: no**, every run — confirms the mutual-exclusion property held.
- No deadlocks across any of the 5 runs.

---

## Problem 4 — Thread pool

`thread_pool.c`

Fixed pool of 4 worker threads pulling from a shared FIFO linked-list queue (mutex + `task_available` condition variable). A second condition variable (`all_done`) plus an `pending` in-flight counter lets `pool_wait()` block the caller until every submitted task has actually finished — not just been dequeued, but fully executed. Workers block on the queue via `pthread_cond_wait` when idle (not busy-polling), and `pool_destroy()` sets a `shutting_down` flag and broadcasts so every idle worker wakes up, sees no more work is coming, and exits cleanly.

Tested with 500 tiny tasks, each writing to its own private slot in a `ran_flags[]` array (so there's no race on the "did this task run" check) plus a mutex-protected shared counter. Ran 5 times: **`Tasks completed: 500`** and **`Every task ran exactly once: YES`**, every single run.

---

## One thing that surprised me

How little extra code the semaphore version of the bounded buffer needed compared to Week 2's condition-variable version — the *logic* is almost identical, but removing the `while` re-check loop (because the semaphore count already encodes "is it safe to proceed") made the correctness argument feel much more mechanical and less "did I get the loop condition exactly right."

## What I got stuck on

Choosing a readers-writers preference policy took some thought — my first instinct was reader preference (simpler to reason about, since a reader never has to check anything about writers waiting), but working through what happens under continuous read load made it obvious that's a real starvation bug for writers, so I switched to writer preference and documented the tradeoff instead of pretending there isn't one.
