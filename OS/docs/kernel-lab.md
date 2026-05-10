# ZenithOS Kernel Lab

Kernel Lab is the preserved from-scratch OS prototype. It is not being deleted,
because it is useful for learning and for experiments that may later inform
ZenithOS internals.

## Preserved Source

- `boot/`
- `kernel/`
- `userland/`
- `tools/package_userland.py`
- `docs/limine-bootstrap.md`
- root `Makefile` kernel targets

## Current Capabilities

- Limine boot into a freestanding x86_64 kernel.
- Serial logging and ASCII boot banner.
- PMM, VMM, page-backed kernel heap.
- GDT/TSS, IDT, timer, keyboard, and mouse paths.
- Scheduler, blocking/wakeup, IPC, syscall entry.
- Initramfs user programs and a small graphical session prototype.

## Commands

```sh
make kernel-lab
make kernel-lab-iso
make kernel-lab-run
```

The compatibility targets `make`, `make iso`, and `make run` still point to
Kernel Lab.

## Product Boundary

ZenithOS Workstation now uses Linux/GNOME internals. Kernel Lab should not block
Workstation milestones, and Workstation should not depend on Kernel Lab code.
