# Week 7 — Problem 1: Address-space design trace

**Source checkout:** `mit-pdos/xv6-riscv` commit `79fa703ee1df7b548ef0bae4f834c453ac75319a`, the same revision used for my Weeks 5 and 6 submission patches.

## 1. What `uvmcopy()` does

`kernel/vm.c:297–323` walks every virtual page below `sz`. For each present mapping it:

1. Allocates a distinct physical page with `kalloc()` (`kernel/vm.c:311`).
2. Copies one page of the parent's contents into that new page with `memmove()` (`kernel/vm.c:313`).
3. Installs a child PTE mapping the same virtual address to the new physical page, preserving the parent's PTE flags, with `mappages()` (`kernel/vm.c:314`).

Thus a successful `fork()` gets an independent address space: identical bytes initially, but different physical pages.

## 2. Failure path in `fork()`

`fork()` calls `uvmcopy(p->pagetable, np->pagetable, p->sz)` in `kernel/proc.c:266`. If it returns a negative value, lines `267–269` call `freeproc(np)`, release the child's lock, and return `-1`. `freeproc()` owns cleanup of the partially created child, including its trapframe and page table. The rule is that a multi-step operation must unwind every resource it has acquired when a later step fails; it cannot leave a `USED` proc slot or partially allocated address space behind.

## 3. Design for a sharing child

At the current `uvmcopy()` call site, a thread-creation path would not call `uvmcopy()`. It would arrange for the child to refer to the same address-space object as the parent, rather than make a byte-for-byte duplicate. In the most literal design, `np->pagetable` would be the exact same pointer value as `p->pagetable`, and `np->sz` would equal the group's shared address-space size.

That makes lifetime management the important new mechanism. A single `pagetable_t` cannot be freed when either one of its users exits. The shared address-space object needs a reference count: increment it when a thread joins the group, decrement it when one leaves, and call the existing page-table freeing path only when the count reaches zero. Keeping that count next to a dedicated address-space/group object is safer than treating one arbitrary `struct proc` as permanent owner, because the original process may exit before its threads.

The ordinary `fork()` path should remain unchanged: it calls `uvmcopy()`, gets a distinct page-table pointer, and assigns the copied size. A new thread path is what would share both the page table and the size/lifetime metadata.

## 4. Why `p->sz` must be shared too

If two threads share page tables but carry independent `p->sz` values, either can make the shared mapping larger while the other still believes the old boundary. The stale thread can then fail argument/range validation for addresses that are actually mapped, or later allocate a stack/heap region based on its stale end and collide with an existing mapping. `copyin`/`copyout` and related user-address validation depend on a correct process size, so `sz` must describe the group-wide address space, not one thread's private view.
