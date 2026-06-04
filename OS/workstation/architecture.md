# ZenithOS Workstation Architecture

## Core Principle

ZenithOS uses standard Linux desktop interfaces underneath and replaces the
visible experience with Zenith-owned UX. This avoids reimplementing decades of
kernel, driver, graphics, accessibility, package, and session work before we
can ship a real desktop.

ZenithOS also targets self-hosting. The live ISO includes the source tree and
`zenith-build`, so the normal build process happens inside ZenithOS after the
first seed image exists.

ZenithOS follows a package/repository/compose model. Owned components are built
as `.deb` packages, published into a Zenith apt repository, and installed into
the image through apt. That mirrors how large distributions separate package
building from image composition.

## Layers

```text
Hardware
Linux kernel and firmware
systemd, udev, logind, journald
glibc, coreutils, apt, dpkg
dbus, polkit, xdg portals
NetworkManager, PipeWire, WirePlumber, upower
Wayland, KWin, KDE Plasma internals
GTK4, libadwaita, VTE, GLib/GIO
ZenithShell and first-party apps
Zenith package repository
Self-hosted image builder
```

## What Zenith Owns

- Desktop visual identity.
- Shell layout, overview behavior, app grid, dash, and system menu.
- Settings, Packages, Terminal, Files, and Installer apps.
- Default package set and storage budget.
- Theme tokens and wallpaper.
- Recovery and update UX.
- In-ISO rebuild workflow through `zenith-build`.

## What Zenith Reuses

- Linux kernel and drivers.
- systemd service/session model.
- apt/dpkg package installation.
- Flatpak sandbox/runtime model.
- KWin/Wayland compositor internals.
- GTK/libadwaita app platform.
- KDE Plasma configurations where they define system behavior.

## First Milestone

Boot a rootfs to SDDM on Wayland, apply Zenith
theme/defaults, launch Zenith Settings from the kickoff launcher, and run
`zenith-build check` inside the system.

## Later Milestones

1. Replace more KDE Plasma surface area with ZenithShell code.
2. Add Zenith Packages as the apt/Flatpak front end.
3. Add installer and recovery snapshot flow.
4. Tune Lenovo V14-ADA hardware defaults.
5. Build a release ISO.
