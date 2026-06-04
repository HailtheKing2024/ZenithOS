# ZenithOS Workstation Boot Sequence

This is the product boot path for the Linux/KDE Plasma-internals ZenithOS
Workstation. The old Limine custom-kernel path is preserved separately as
Kernel Lab.

## 1. Firmware

UEFI initializes the Lenovo V14-ADA platform, exposes ACPI tables, initializes
the internal panel, and hands control to the bootloader.

## 2. Bootloader

GRUB or systemd-boot loads the Linux kernel, initramfs, and kernel command line.
Secure Boot support is a later hardening gate, but the boot path must already
avoid unsigned third-party boot artifacts in release images.

## 3. Linux Kernel

The kernel initializes AMD64 CPU state, memory management, ACPI, PCI, storage,
AMDGPU, input, Wi-Fi, audio, battery, and power-management interfaces. Device
events flow through udev, and the graphical stack uses DRM/KMS rather than the
old Kernel Lab GOP framebuffer path.

## 4. Early Userspace

The initramfs locates the root filesystem, mounts it read/write or read-only
depending on recovery mode, then transfers control to systemd.

## 5. systemd

systemd starts udev, journald, dbus, logind, polkit, NetworkManager, PipeWire,
WirePlumber, SDDM, and Zenith-specific services. Unit ordering must keep shell
startup blocked until the session bus, logind, and display manager are ready.

## 6. Display Manager

SDDM starts a Wayland session. The first release can use KDE Plasma internals
with Zenith configurations and Zenith defaults. Later releases can replace
more shell surface area while keeping KWin, Wayland, portals, and Qt/GTK.

## 7. ZenithShell

ZenithShell owns the visible desktop experience:

- top bar
- activities overview
- search
- workspace and window overview
- app grid
- system status menu
- first-party app launchers
- theme and motion tokens

## 8. First-Party Apps

Zenith Settings, Terminal, Files, Packages, and Installer use GTK4/libadwaita
and communicate with system services through dbus, polkit, NetworkManager,
PipeWire, Flatpak, apt, and xdg portals.

## 9. Self-Hosted Builder

The live ISO includes `/usr/bin/zenith-build` and stages the source tree at
`/usr/src/zenithos`. After boot, ZenithOS can validate its own manifests and
build the next root filesystem from inside the OS:

```text
zenith-build check -> zenith-build rootfs -> zenith-build iso
```

The first seed image still needs an outside Linux environment, but later images
should be built from a running ZenithOS session.

## Verification Gate

The Workstation boot cycle is valid when a built image reaches:

```text
UEFI -> bootloader -> Linux -> initramfs -> systemd -> SDDM -> Wayland -> ZenithShell -> app launcher -> zenith-build check
```

The installed footprint must remain under 30GB and `apt` must be the system
package manager.
