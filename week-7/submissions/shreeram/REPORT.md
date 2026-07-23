# Week 7 Report — Thread Design

**Submitted by:** shreeram  
**Problems attempted:** 1–4  
**Checkout targeted:** `mit-pdos/xv6-riscv` commit `79fa703ee1df7b548ef0bae4f834c453ac75319a`, with my Week 5 and Week 6 syscall additions already applied.

## Implementation

`kernel-diffs.patch` adds `is_thread` and `tgid` to `struct proc`, initializes each new process as an ordinary one-member thread group, and adds three syscalls:

- `getthreadinfo(struct thread_info *)`
- `alloc_thread_stack(int)`
- `alloc_thread_stack_guarded(int)`

The syscall numbers are 28–30 because Weeks 5 and 6 already use 22–27. The patch also adds the matching user stubs and declarations. Add the three test programs to xv6's `UPROGS` list as `_threadinfo_test`, `_stack_alloc_test`, and `_guardpage_test` before building.

## Problem 1

See `thread_design.md`. In brief, `uvmcopy()` allocates, copies, and maps a new physical page for each parent page. A genuine thread path must instead share one address-space object and reference-count it; the page-table pointer and address-space size must be group-wide rather than independently copied fields.

## Problem 2

For a plain `fork()`, each process is an ordinary one-member group: `is_thread == 0` and `tgid == pid`. This happens because `allocproc()` assigns a fresh PID first, then initializes `tgid` from that PID; `fork()` does not overwrite either field. `threadinfo_test.c` checks this in both parent and child.

## Problem 3

`alloc_thread_stack()` rounds the requested size to pages, grows the calling address space with `uvmalloc()`, commits the returned size to `p->sz`, and returns that new top as the downward-growing stack pointer. Two 4096-byte calls return tops exactly 4096 bytes apart, so the stacks are adjacent. Without a guard page, overflow below a stack can silently corrupt data below it instead of faulting.

## Problem 4

The guarded version reserves one extra page, then unmaps and frees the lowest newly allocated page. For a page-aligned request of 4096 bytes, the usable stack is exactly 4096 bytes: `[top - 4096, top)`. `top - 4096 - 1` is inside the unmapped guard page and should kill only the test process with an xv6 `usertrap(): unexpected scause ... 0xf` store-page-fault diagnostic. `guardpage_test.c` deliberately performs this write after proving `top - 1` works.

One implementation detail worth noting: both allocators round the old boundary up to a page before creating a new stack region. `uvmunmap()` requires a page-aligned virtual address; this also makes the guard-page invariant safe even if a caller previously used `sbrk()` with a non-page-sized increment.

## Verification status

The package is structured like the prior weeks (`week-7/submissions/shreeram/`): separate tests, written trace/report, and an apply-ready kernel patch. The repository itself has no xv6 checkout or RISC-V/QEMU toolchain, so the final QEMU build/run still needs to be performed in the user's xv6 environment.
