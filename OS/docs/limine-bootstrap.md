# Limine Bootstrap Milestone

This document defines the first ZenithOS boot milestone: build a Limine-based
UEFI ISO, run it under QEMU, and prove that the kernel entry point is reached.
It is intentionally limited to bootstrap validation; memory management,
drivers, filesystems, and the compositor are later milestones.

## Toolchain Prerequisites

Install the host tools before attempting the first boot:

- `x86_64-elf-gcc` or `clang`/`lld` capable of producing freestanding
  `x86_64` ELF64 objects.
- `nasm` for any early assembly entry stubs.
- `make` or `ninja` for repeatable local builds.
- `limine` bootloader files and deployment tool.
- `xorriso` for ISO image creation.
- `qemu-system-x86_64` for first-boot validation.
- OVMF UEFI firmware, normally exposed as `OVMF_CODE.fd`.

The kernel must be built freestanding: no host libc, no C runtime startup, no
implicit stack protector runtime, and no accidental host-specific libraries.
Pin the Limine release used by the build; do not build production artifacts from
an unpinned moving branch.

## Expected Build Commands

The first build interface should stay small and stable:

```sh
make toolchain-check
make iso
make run
```

Where:

- `make toolchain-check` verifies the required tools are present.
- `make iso` produces `build/zenithos.iso`.
- `make run` boots `build/zenithos.iso` through QEMU with UEFI firmware.

Until those targets exist, the intended QEMU shape for the milestone is:

```sh
qemu-system-x86_64 \
  -machine q35 \
  -m 1024M \
  -cpu max \
  -bios path/to/OVMF_CODE.fd \
  -cdrom build/zenithos.iso \
  -boot d \
  -serial stdio
```

Keep serial output enabled from the start. It gives the kernel a reliable
validation channel before the framebuffer, font renderer, and compositor exist.

The ISO creation step behind `make iso` should follow this shape:

```sh
xorriso -as mkisofs -R -r -J \
  -b limine-bios-cd.bin \
  -no-emul-boot -boot-load-size 4 -boot-info-table \
  -hfsplus -apm-block-size 2048 \
  --efi-boot limine-uefi-cd.bin \
  -efi-boot-part --efi-boot-image --protective-msdos-label \
  build/iso_root \
  -o build/zenithos.iso

limine bios-install build\zenithos.iso
```

## ISO Layout

The generated ISO should contain only the minimum boot surface:

```text
/
|-- limine.conf
|-- limine-bios.sys
|-- limine-bios-cd.bin
|-- limine-uefi-cd.bin
|-- EFI/
|   `-- BOOT/
|       `-- BOOTX64.EFI
`-- boot/
    `-- zenithos.elf
```

The initial `limine.conf` should point directly at the kernel ELF and use a
clear milestone label, for example `ZenithOS bootstrap`.

```ini
timeout: 5
default_entry: 1
interface_branding: ZenithOS

/ZenithOS bootstrap
protocol: limine
path: boot():/boot/zenithos.elf
cmdline: milestone=bootstrap
```

## First Boot Validation

The first boot is valid only when all of the following are true:

- QEMU reaches Limine without dropping to the UEFI shell.
- Limine shows the `ZenithOS bootstrap` entry.
- Selecting the entry transfers control to the kernel.
- The kernel writes a deterministic message to the framebuffer, serial port, or
  both, for example `ZenithOS: kernel entry reached`.
- The kernel halts cleanly after the message instead of triple-faulting or
  rebooting.

For this milestone, a static "hello from kernel entry" screen is enough. The
important proof is that ZenithOS owns execution in 64-bit long mode through a
reproducible Limine boot path.

## Failure Triage

- UEFI shell appears: confirm the ISO contains `EFI/BOOT/BOOTX64.EFI` and QEMU
  is using OVMF.
- Limine menu appears but the kernel does not start: confirm the kernel path in
  `limine.conf` matches the ISO layout and the ELF is linked for the expected
  higher-half or physical entry convention.
- Immediate reboot or freeze: capture `-serial stdio`, verify the entry symbol,
  and check that the early stack, page-table assumptions, and halt loop are
  valid for the chosen Limine protocol.

## Milestone Exit Criteria

This milestone is complete when `make iso` and `make run` are reproducible from
a clean checkout, and a screenshot or serial log shows the kernel entry message.
No memory allocator, scheduler, PCI scan, or graphics compositor work is
required to close this milestone.
