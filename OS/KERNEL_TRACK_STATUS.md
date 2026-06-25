# ZenithOS Hybrid Kernel Track — Status

**Last touched:** May 7, 2026 (initial commit)
**Status:** Scaffolded only. No ISO ever booted from `kernel/`, `userland/`, or
`boot/`. No commits since the original cut except `ffc8ae8 Refactor ZenithOS
boot and memory initialization` (June 4).

## Why we're not building it right now

The repository's actively shipping track is the Debian + KDE Plasma
**workstation** ISO (`OS/workstation/`, ~1.16 GB seed ISO last built
June 11, 2026). It satisfies the same product goal — a Fedora-style
workstation optimised for the Lenovo V14-ADA — using mature,
production-grade plumbing so that the ZenithOS-specific UI/apps/install
work can ship against a real running system instead of being blocked
on kernel bootstrapping.

The hybrid kernel track is the more ambitious but slower path. We will
return to it once the workstation track is feature-complete enough to
not need monthly kernel-level rework.

## What exists

```
OS/boot/limine.conf         # Limine bootloader stub
OS/kernel/                  # Filled with header files and stub .c
  arch/x86_64/              # a few .S / .c files; never compiled past Make
  fs/ gfx/ ipc/ sched/ sys/ time/ user/
OS/kernel/main.c            # 210 lines, prints banner, initialises PMM/VMM
OS/userland/                # sh, ls, app launcher, terminal.c (35K lines, audit-ed)
```

The Makefile defines `kernel`, `iso`, and `run` targets that reference
the Limine BIOS install tool. None of them have been verified end-to-end
on real hardware.

## What doesn't exist

- IDT/GDT/IRQ plumbing wired into `main.c`.
- PIT timer driven scheduler ticks.
- VFS actually mounting a disk.
- Syscall table populated against `userland/`.
- A framebuffer with a tested desktop shell.

## When to reopen this track

Restart the kernel work when **any** of these becomes true:

1. The workstation track has shipped public Beta 3 with installer + hardware validation complete.
2. A specific ZenithOS feature genuinely needs privileged isolation that the
   microkernel/hybrid layout would unlock.
3. A standalone research block is carved out — 1–2 weeks minimum — with no
   concurrent workstation commitments.

Until then, treat `kernel/` + `userland/` as a research folder, not part of
the product release checklist.

## Pointers

- Original design: `OS/AGENTS.md`.
- Distinction between tracks: `OS/workstation/README.md` ("Workstation
  is the real desktop OS track").
