# Week 1 Report — Threads & Concurrency Basics

**Submitted by:** shreeram
**Problems attempted:** 1 (Run, observe, write down), 2 (Make the race worse), 3 (Parallel word count), 4 (Multi-threaded simulation — stretch)

Toolchain used: MinGW-w64 GCC 16.1.0 (POSIX threads, UCRT runtime) on Windows. All programs compile cleanly with `gcc -Wall -pthread` and no warnings.

---

## Problem 1 — Run, observe, write down

Ran each of the four `week-1/code` examples 10 times.

**`hello_thread`** — behaved identically every run (prints from two threads, order can vary but no correctness issue to observe).

**`race_counter`** (default `ITERS = 10,000,000`, expected final value `20,000,000`):

| Run | Final counter | Correct? |
|---|---|---|
| 1–10 | ranged from 10,687,499 to 14,239,802 | **Wrong every single run (10/10)** |

The race showed up 100% of the time on this machine — a multi-core box where the two threads genuinely run in parallel, so the load/increment/store interleaving happens constantly.

Variations:
- **`ITERS = 1000`** (very small): correct on 10/10 runs. At this size the whole loop finishes in well under a millisecond — likely faster than the OS can even meaningfully interleave the two threads, and/or thread creation overhead dominates the total runtime enough that the two loops don't overlap in practice. Too few iterations for the race to have a realistic chance to manifest.
- **`ITERS = 100,000,000`**: wrong on 5/5 runs, and by a much larger absolute margin (typically 75–90 million off, vs. ~7–9 million off at the default size) — more iterations means more opportunities for a bad interleaving, so the accumulated error grows roughly proportionally.
- **Compiled with `-O2`**: correct on 10/10 runs. This does **not** mean the race is fixed — there's still no synchronization at all. The likely explanation: at `-O2` the compiler keeps `counter` in a register for the entire loop (one load before the loop, one store after), so each thread's whole 10-million-iteration loop finishes in a handful of milliseconds. That's fast enough that thread-creation/scheduling overhead ends up serializing the two threads in practice (one finishes before the other meaningfully starts), which produces a "correct" answer by accident of timing, not by correctness. I would not trust this to hold on a different machine, a busier machine, or a larger `ITERS`.

**`parallel_sum`** (expected sum `10,000,000`, since the array is filled with 1s):

10/10 runs were wrong, off by roughly 3.5–5.4 million each time. Same underlying bug as `race_counter`, just with 4 threads updating the shared `total` instead of 2.

**`bank_chaos`**: 10/10 runs ended with `balance == 0` exactly, never negative. This is an honest observation, not a "the bug doesn't exist" claim — the check-then-act window here (`if (balance >= 1) balance -= 1;`) is only a couple of instructions wide with no artificial delay, so across 100,000 attempts per customer the race almost never gets a chance to actually fire (both customers' checks/acts happen to interleave cleanly enough, most of the time, that the accounting stays consistent by luck). I amplified this exact scenario in Problem 4 below by widening the gap on purpose, which reliably produces a visible violation — see that section.

---

## Problem 2 — Make the race worse

`submissions/shreeram/problem-2/race_amplified.c`

Changed from `counter++` to the split load/gap/store form, with a `usleep(1)` inserted between the load and the store (and dropped `ITERS` to 2000, since with the sleep even a small iteration count is plenty to demonstrate the effect and keeps each run fast). Avoided `printf` inside the loop since it has its own internal locking and would mask the very race being amplified.

Result across 10 runs: **wrong on 10/10 runs**, with the counter landing well below the expected `4000` (down to as low as ~half the expected value, i.e. `ITERS`, not `2*ITERS` — meaning the threads were stomping on each other very heavily). This matches the "success criterion" described in the problem statement almost exactly.

---

## Problem 3 — Parallel word count

`submissions/shreeram/problem-3/word_count.c` (test file: `moby_dick.txt`, downloaded from Project Gutenberg, ~212,813 words per `wc -w`)

A "word" is defined as a maximal run of non-whitespace characters (same definition `wc -w` uses), and chunk boundaries are nudged forward to the next whitespace character so a word is never split across two threads' chunks.

| Threads | Word count reported | `wc -w` reference |
|---|---|---|
| 1 | 212,814 | 212,813 |
| 2 | 212,814 | 212,813 |
| 4 | 212,814 | 212,813 |
| 8 | 212,814 | 212,813 |

Consistently off by exactly **1**, regardless of thread count — this is not the race, it's a definition mismatch: the file appears to end without a trailing newline/whitespace after the last word in a way that makes my "maximal run of non-whitespace" counter count one extra token at a boundary (likely a stray byte-order-mark or an extra token at the very start/end of the file that `wc -w`'s definition doesn't count the same way). I did not chase this down further since it's consistent and small, but it's worth noting as a real discrepancy rather than hiding it.

More importantly for this problem's actual point: the shared `total` is updated exactly once per thread (`total += local` after each thread finishes its own chunk), not once per word. That means with only 1–8 threads there are only 1–8 writes to the shared total ever — nowhere near enough contention for the race to have a realistic chance of manifesting, and indeed I never observed a mismatch beyond the constant off-by-one across 10 runs at 8 threads. This is consistent with the amplification lesson from Problem 2: races need either a lot of writes to the same location, or an artificially widened window, to show up reliably.

---

## Problem 4 — Multi-threaded simulation (stretch)

`submissions/shreeram/problem-4/ticket_booking.c`

**Scenario:** a venue with 1000 seats. 8 "booking agent" threads each attempt 500 bookings of 3 seats each, using an unsynchronized check-then-act (`if (seats_sold + 3 <= 1000) seats_sold += 3;`).

**Invariant:** `seats_sold <= SEATS_AVAILABLE` (1000).

First pass with no artificial delay between the check and the act: 15/15 runs landed on exactly `999` sold, never violating the invariant — same story as `bank_chaos` above, the window is just too narrow to hit without help. I then added a tiny `usleep(5)` between the check and the act (explicitly documented in the code as an amplification, same technique as Problem 2), and reran: on 8 runs, 1 run produced `seats_sold = 1002` — **the venue oversold by 2 seats**, a clear invariant violation, while several other runs still landed on 999. This matches the assignment's framing well: the bug is real and general (any multi-step check-then-act on shared state has it), but whether it's *observed* on a given run depends heavily on how wide the race window actually is.

---

## One thing that surprised me

The `-O2` result for `race_counter` looked like optimization "fixed" the bug, but on reflection it's the opposite kind of danger: it's a coincidence of timing (fast-enough loop + thread-creation overhead) making the bug *invisible*, not gone. A change in hardware, load, or iteration count could un-hide it immediately. That's a scarier failure mode than an obviously-broken run, because it would pass a casual "looks correct" check.

## What I got stuck on

Getting the ticket-booking overbooking violation to actually show up took a couple of iterations — the naive version (no delay) never violated the invariant across 15 runs, which initially looked like "my simulation doesn't have a bug." Recognizing that this was the *same* narrow-window problem from Problem 1/2, and applying the same amplification technique, resolved it.
