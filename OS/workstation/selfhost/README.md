# ZenithOS Self-Hosting

ZenithOS Workstation should rebuild itself from inside the live ISO or an
installed ZenithOS system.

There is one unavoidable bootstrap exception: the very first image must be
created by an existing Linux environment. After that, the normal build path is:

```text
Boot ZenithOS ISO
Open Zenith Terminal
Run zenith-build check
Run zenith-build rootfs
Run zenith-build iso
Boot the newly produced ISO
```

## Installed Paths

The image builder installs:

```text
/usr/bin/zenith-build
/usr/src/zenithos
/var/lib/zenith-build
```

`/usr/src/zenithos` contains the source manifests, shell extension, app
scaffolds, and image scripts needed for the next build.

## Builder Tools In The ISO

The rootfs manifest includes the tools required for self-hosted builds:

- apt/dpkg
- make
- git
- rsync
- mmdebstrap
- debootstrap
- squashfs-tools
- xorriso
- GRUB/EFI image tools
- dosfstools, mtools, parted, gdisk
- qemu-system-x86 for local boot tests

## Current Builder Scope

`zenith-build rootfs` builds the next root filesystem with `mmdebstrap`.
`zenith-build iso` assembles a live ISO from that rootfs with SquashFS and GRUB.
The next engineering gate is boot-testing that ISO in QEMU from inside ZenithOS.

## Storage Rule

Build outputs go to `/var/lib/zenith-build` by default. The live ISO and
installer must expose cleanup controls so old build outputs do not consume the
30GB target.
