# Workstation Image Builder

This directory contains the Linux rootfs/image build entry points.

The first script builds a root filesystem. The second script assembles a live
ISO from that rootfs.

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
