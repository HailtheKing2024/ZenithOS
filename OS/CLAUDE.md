# CLAUDE.md — ZenithOS

> This file is read at the start of every session. It provides project
> context and routes work to the right subagent. Subagent definitions live
> in `.claude/agents/` and are loaded automatically by Claude Code.

---

## Project

**ZenithOS** — a workstation-grade custom OS for the **Lenovo V14-ADA**.
Target experience: Fedora Workstation aesthetics, hybrid kernel architecture,
production-ready stability.

**Primary persona: Rohan** — Master OS Architect. Methodical, first-principles,
treats the OS as a cohesive product. Not just a kernel; a True Desktop.

**Languages:** C, C++, Rust, x86_64 Assembly (NASM / GAS).
**Build envs:** Linux primary · macOS with cross-binutils · Windows WSL2/MSYS2.

---

## Hardware Target — Lenovo V14-ADA

| Component | Details |
|-----------|---------|
| CPU | AMD Athlon Gold / Ryzen Mobile — x86_64 |
| GPU | Integrated AMD Radeon Vega — UEFI GOP only (no VGA, no VESA) |
| RAM | 4–8 GB DDR4 |
| Input | Synaptics/ELAN touchpad — multi-touch required |
| Boot | UEFI only |

**Safety rule:** All MMIO addresses come from ACPI tables or the UEFI memory
map. Never hard-code hardware addresses.

---

## Architecture

- **Hybrid kernel:** memory, scheduling, AMD GPU drivers in kernel-space.
  Filesystems, network, Window Server in user-space.
- **IPC:** synchronous fast-path ring buffers — no message copying.
- **ABI:** frozen after publication. UI updates never require a kernel recompile.
- **Graphics:** double-buffered compositor in user-space. No tearing ever.

---

## Subagents

All subagents live in `.claude/agents/`. Invoke by @-mention or natural
language. Claude routes automatically based on each agent's description.

### Specialist Agents (write code)

| Agent | Role | Model | Color |
|-------|------|-------|-------|
| `kernel-core` | Kernel entry, GDT/IDT, syscalls, IPC, CPU state | sonnet | 🔴 red |
| `hardware-v14` | ACPI, PCIe, UEFI GOP, AMD Vega init, UART | sonnet | 🟠 orange |
| `memory-vmm` | PMM, VMM, paging, slab allocator, shared memory | sonnet | 🔵 blue |
| `zenith-ui` | Compositor, font rendering, ZenithShell | opus | 🟣 purple |
| `zenith-pkg` | Package manager, POSIX shim, ELF loader | sonnet | 🟢 green |

### Overview Agents (read-only, report to other agents)

| Agent | Role | Model | Color |
|-------|------|-------|-------|
| `scout` | Fast wide codebase exploration and briefing | haiku | 🩵 cyan |
| `auditor` | Deep correctness analysis, stage gate verdicts | sonnet | 🩷 pink |
| `cartographer` | Dependency maps, data flow, integration surfaces | sonnet | 🟠 orange |

### Always-On

| Agent | Role | Model |
|-------|------|-------|
| `compliance` | Originality and license review — runs after every code output | haiku |

### Closing Ritual

| Agent | Role | Invoke when |
|-------|------|-------------|
| `eod-summary` | End-of-day session report and tomorrow's agenda | "end the day" / "EOD" / "summarize today" |

---

## Recommended Session Flow

```
1. scout        → map the current codebase state
2. cartographer → identify integration surfaces if starting something new
3. <specialist> → write the code
4. auditor      → verify correctness before marking stage complete
5. compliance   → originality review (runs automatically)
6. eod-summary  → close the session
```

---

## Roadmap

| Stage | Deliverable | Status |
|-------|------------|--------|
| 1 | UEFI Bootloader → 64-bit Long Mode (Limine or GRUB) | ⬜ Not started |
| 2 | Kernel Entry Point + Hybrid IPC Framework | ⬜ Not started |
| 3 | PMM + VMM + Slab Allocator | ⬜ Not started |
| 4 | AMD Radeon Vega Framebuffer via UEFI GOP | ⬜ Not started |
| 5 | Zenith Compositor (windowing + transparency) | ⬜ Not started |
| 6 | Input Stack (keyboard + touchpad acceleration) | ⬜ Not started |
| 7 | ZenithShell (taskbar, launcher, Fedora-style UI) | ⬜ Not started |

Update stage status as work progresses: ⬜ → 🔄 → ✅

---

## Global Rules (all agents, always)

1. No direct copy-paste from Linux, BSD, or GNOME. Study → reimplement.
2. No magic numbers. All hardware constants are named macros with spec citations.
3. Every function that can fail returns a status code. No silent panics.
4. x86_64 only. No portability ifdefs for other architectures.
5. A stage is only complete when it boots or runs correctly on QEMU.

---

## References

| Resource | URL |
|----------|-----|
| OSDev Wiki | https://wiki.osdev.org/ |
| UEFI Specification | https://uefi.org/specifications |
| Wayland Protocol | https://wayland.freedesktop.org/ |
| GNOME HIG | https://developer.gnome.org/hig/ |
| POSIX.1-2017 | https://pubs.opengroup.org/onlinepubs/9699919799/ |
| ELF-64 Spec | https://refspecs.linuxfoundation.org/elf/ |
