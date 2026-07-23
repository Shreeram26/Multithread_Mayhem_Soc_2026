# Week 5, Problem 1 — Trace a syscall by hand

**Syscall traced:** `sys_getpid` (via a user program calling `getpid()`)
**Checkout used:** `mit-pdos/xv6-riscv` @ commit `79fa703ee1df7b548ef0bae4f834c453ac75319a` (2022-09-09) — see `REPORT.md` for why this specific commit rather than current `HEAD`.

---

## 1. The user-space stub

`user/usys.pl` (lines 9–16, 35) is a Perl script that *generates* `user/usys.S` at build time — it's not committed source, it's a build artifact, which is why `theory.md` points at both files. The generated stub for `getpid` looks like:

```asm
.global getpid
getpid:
 li a7, SYS_getpid
 ecall
 ret
```

Two things happen before the trap: **(1)** `li a7, SYS_getpid` loads the syscall number (`11`, from `kernel/syscall.h` line 12) into register `a7` — this is how the kernel will later know *which* syscall was requested. **(2)** `ecall` is the actual trap instruction.

## 2. The `ecall` instruction

`ecall` is not xv6 source code — it's a single RISC-V instruction, so this checkpoint is about hardware behavior, not a line to cite. Executing it from user mode: the privilege level switches from U-mode to S-mode; `sepc` (supervisor exception PC) is loaded with the address of the `ecall` instruction itself; `scause` (supervisor cause register) is set to `8`, the RISC-V-defined code for "environment call from U-mode"; and control transfers to whatever address is in `stvec` (supervisor trap vector) — which xv6 has previously pointed at `uservec` in the trampoline page.

## 3. The first xv6 instruction that runs afterward

`kernel/trampoline.S`, `uservec` (lines 22–98). It's still running with the *user* page table active at this point (the trampoline page is mapped at the same virtual address in every process's page table for exactly this reason). It saves `a0` into `sscratch` momentarily, then loads `a0 = TRAPFRAME` and saves every other user register into the fixed offsets of the per-process trapframe (lines 40–69: `ra`, `sp`, `gp`, `tp`, `t0-t2`, `s0-s11`, `a1-a7`, `t3-t6`); then it recovers the saved user `a0` from `sscratch` and stores it too (lines 72–73). Only after all user registers are safely in the trapframe does it load the kernel stack pointer, hartid, and `usertrap`'s address out of the trapframe's `kernel_*` fields (lines 76–82), switch to the kernel page table (line 92), and finally `jalr t0` to actually call `usertrap()` (line 98).

## 4. The C function that decides this is a syscall

`usertrap()` in `kernel/trap.c` (lines 37–94). The condition it checks is at **line 54**: `if(r_scause() == 8){` — exactly the value hardware set in `scause` back at checkpoint 2. Inside that branch it also bumps `p->trapframe->epc += 4` (line 62) so that when the process eventually resumes, it resumes at the instruction *after* `ecall`, not `ecall` itself again (which would just trap forever).

## 5. Where the syscall number is read and the handler found

`syscall()` in `kernel/syscall.c` (lines 131–147). **Line 137**: `num = p->trapframe->a7;` reads the number the user stub loaded into `a7` back at checkpoint 1. **Line 138** checks it's in range and non-null, then **line 141**: `p->trapframe->a0 = syscalls[num]();` — the `syscalls[]` array (lines 107–129) is indexed directly by the syscall number (`[SYS_getpid] sys_getpid` at line 118), so no separate lookup table or `switch` is needed.

## 6. The actual implementation

`sys_getpid()` in `kernel/sysproc.c` (lines 18–22):

```c
uint64
sys_getpid(void)
{
  return myproc()->pid;
}
```

One line in the body. It takes no arguments (no `argint`/`argaddr` calls needed) and returns `myproc()->pid` — the calling process's own PID, read directly out of its `struct proc`. The return value ends up in `p->trapframe->a0` (set by `syscall()` at checkpoint 5, line 141) — the same register the RISC-V calling convention uses for a C function's return value, which is exactly why it "just works" as an ordinary-looking return value once we're back in user space.

## 7. The return path

There is **no separate `usertrapret()` function in this specific commit** (worth calling out — some later/other xv6 revisions do split this into its own function, and `theory.md` may describe it that way). Here, the "prepare for return" work is a helper called `prepare_return()` (`kernel/trap.c`, called at the end of `usertrap()`, line 87), which sets up `sstatus` (clears `SPP` for user mode, sets `SPIE` to re-enable interrupts) and `sepc` (to the saved `p->trapframe->epc`). `usertrap()` then returns the user's `satp` value; because `uservec` called it with `jalr` (not a jump), control falls straight through to the very next label in `trampoline.S`, which is **`userret`** (line 100) — so the two pieces of code involved are `usertrap()`/`prepare_return()` (C) and `userret` (assembly). `userret` switches back to the user page table (line 107), restores every user register from the trapframe except `a0` (lines 113–142), restores `a0` last (line 145, since it held the *new* `satp` value on entry and now needs to become the syscall's actual return value), and finally executes **`sret`** (line 149) — the instruction that actually drops the CPU back to U-mode and resumes execution at `sepc` (the instruction right after the original `ecall`, thanks to the `epc += 4` from checkpoint 4).
