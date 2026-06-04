# ZenithOS

ZenithOS is now split into two tracks:

1. **ZenithOS Workstation** is the product target: a custom Linux distribution
   using Linux kernel, systemd, apt/dpkg, Flatpak, Wayland, KWin/KDE Plasma
   internals, GTK, and libadwaita. The visible desktop, settings, terminal,
   package UI, theme, defaults, and installer are Zenith-owned.
2. **ZenithOS Kernel Lab** preserves the existing from-scratch Limine kernel.
   It remains useful for learning kernel internals, schedulers, syscalls,
   framebuffer drawing, and early userspace, but it is no longer the fastest
   path to the real desktop OS.

The guiding product rule is simple: use proven Linux/KDE Plasma interfaces for the
internals, then build a clean modern Zenith desktop on top.

The build rule is also explicit: ZenithOS should become self-hosting. The first
seed image can be bootstrapped by an existing Linux environment, but the normal
workflow after that is to boot ZenithOS and run `zenith-build` from inside the
ISO or installed system.

The release-engineering rule is package-first. Zenith-owned components are
built as `.deb` packages, published into a Zenith apt repository, and then
installed into the image through apt. The image composer should not be the
owner of application files.

## Workstation Target

The Workstation stack is:

```text
UEFI
GRUB or systemd-boot
Linux kernel plus firmware
systemd, udev, journald, logind
glibc, coreutils, apt, dpkg
dbus, polkit, xdg portals
PipeWire, WirePlumber
NetworkManager, iwd
Wayland, KWin, KDE Plasma internals
GTK4, libadwaita, VTE, GLib/GIO
Flatpak
ZenithShell
Zenith Settings, Terminal, Files, Packages, Installer
```

This is not intended to feel like a rebranded base distro. Debian-compatible
packages are the substrate so that apt, drivers, system services, security
updates, and KDE/Qt libraries work correctly. Zenith owns the user experience.

## Repository Layout

```text
workstation/             Product OS track
  apps/                  Zenith first-party app specs and GTK scaffolds
  config/                System defaults layered into the image
  distro/                Package build, apt repo, and compose pipeline
  iso/                   Rootfs and image build scripts
  packages/              apt, Flatpak, KDE-internals package policy
  selfhost/              In-ISO builder command and source staging rules
  shell/                 ZenithShell design and KDE Plasma integration
  storage/               30GB budget and enforcement policy
  themes/                Zenith visual tokens and CSS

kernel/ userland/ boot/  Kernel Lab prototype
docs/kernel-lab.md       Kernel Lab status and preserved build commands
```

## Commands

Validate the Workstation planning/build scaffold:

```sh
make workstation-check
```

Build Zenith-owned packages and publish the local apt repository:

```sh
make workstation-repo
```

Build a Workstation root filesystem from ZenithOS or another Linux host with
`mmdebstrap`:

```sh
make workstation-rootfs
```

On Windows, `make workstation-rootfs` intentionally refuses to run through WSL
by default. If we need a one-time seed image before ZenithOS can build itself,
that bootstrap path is explicit:

```sh
make workstation-rootfs-wsl
```

Inside a ZenithOS live ISO or installed system, the intended build commands are:

```sh
zenith-build check
zenith-build rootfs
zenith-build iso
```

Run the current Workstation seed image from Windows/QEMU:

```powershell
mingw32-make workstation-run-seed
```

The default seed runner uses a direct kernel/initrd boot path with WHPX
hardware acceleration and `qemu64`, disables Plymouth for QEMU stability, and
sets `zenith.profile=dev-vm` so the OS uses the virtual-hardware profile. On
Windows it launches QEMU through `tools/run_workstation_seed.ps1`, so the make
command returns after opening the VM. Full ISO and splash validation remain
separate:

```powershell
mingw32-make workstation-run-seed-iso
mingw32-make workstation-run-seed-splash
```

Run bounded non-interactive QEMU smoke checks:

```powershell
mingw32-make workstation-smoke-seed
mingw32-make workstation-smoke-seed-persistent
```

Create and boot a persistent dev VM disk:

```powershell
mingw32-make workstation-vm-persistence-wsl
mingw32-make workstation-run-seed-persistent
```

Run the old Kernel Lab prototype:

```sh
make kernel-lab-run
```

The existing `make`, `make iso`, and `make run` targets are still preserved for
Kernel Lab compatibility.

## Current Milestone

- Product architecture pivoted to Linux/KDE Plasma internals.
- Self-hosted rootfs and live ISO build architecture added through
  `zenith-build`.
- QEMU seed boot stabilized by using a direct boot path for the default runner.
- Hardware profile detection now separates the dev VM, Lenovo V14-ADA target,
  and unknown generic workstations.
- Live boot speed improved by masking duplicated live-config work and stamping
  composed images as updated.
- ZenithShell, Welcome, Settings, Packages, Files, and Terminal now have usable
  desktop controls instead of only placeholder surfaces.
- ZenithShell now includes a fuller launcher overview, dock, running-window
  switcher, hardware-profile chip, and system controls.
- Zenith Installer now performs non-destructive install readiness checks and
  shows the planned EFI plus Btrfs layout.
- Beta 2 release hygiene adds changelog, known issues, release notes, ISO
  SHA256 generation, and `/etc/os-release` version stamping.
- Kernel Lab preserved without deleting the prototype.
- Package, Flatpak, KDE-internals, and storage policies live under
  `workstation/`.
- ZenithShell shell direction and first-party app scaffolds are being built as
  native KDE/GTK integration layers.
- Installed footprint target remains under 30GB.

## Non-Negotiables

- apt/dpkg is the system package manager.
- Flatpak is supported for apps.
- dnf is not part of ZenithOS Workstation.
- The desktop should be clean, modern, and KDE Plasma-quality without copying KDE/GNOME
  source code.
- Lenovo V14-ADA support is a first hardware target, not the only future target.
