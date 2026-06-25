# Workstation Image Builder

This directory contains the Linux rootfs/image build entry points.

`build-seed-iso.sh` is the current seed entry point: it builds or refreshes the
rootfs, publishes the Zenith apt repo, and assembles the live ISO. The lower
level scripts remain available when rootfs and ISO composition need to be run
separately.

## Requirements

Run on a Linux host or WSL distro with:

- `mmdebstrap`
- `sudo`
- `apt`
- `grub-mkrescue`
- `mksquashfs`
- `xorriso`
- `mtools` (`mformat`)
- network access to configured package repositories

## Rootfs

```sh
./workstation/iso/build-seed-iso.sh build/workstation/zenithos-seed.iso
```

Lower-level flow:

```sh
./workstation/iso/build-rootfs.sh
./workstation/iso/build-live-iso.sh
```

The default output is:

```text
build/workstation/rootfs
build/workstation/zenithos-workstation.iso
```

The script refuses to overwrite an existing rootfs. Remove or move the old
output when intentionally rebuilding.
