# ZenithOS Workstation

ZenithOS Workstation is the real desktop OS track. It uses Linux and GNOME
interfaces internally, while Zenith owns the shell, apps, theme, defaults, and
installer.

## Design Goal

Build a clean modern desktop with the reliability of the Linux/GNOME platform:

- Wayland-first session.
- Mutter/GNOME Shell internals for compositor and session behavior.
- GTK4/libadwaita for first-party apps.
- apt/dpkg for system packages.
- Flatpak for app distribution.
- systemd, dbus, polkit, NetworkManager, PipeWire, and portals.
- Installed footprint under 30GB.

## Directories

```text
apps/       Zenith first-party app scaffolds and product specs
config/     Image defaults installed into /etc, /usr, and dconf
iso/        Rootfs/image build scripts
  packages/   Package and Flatpak manifests
  selfhost/   In-ISO builder command and source staging
  shell/      ZenithShell design and GNOME integration
storage/    30GB budget and enforcement policy
themes/     Visual tokens and CSS
```

## Build Shape

The first build milestone is a root filesystem that can boot to GDM and load a
Zenith session customization. A polished installer and final ISO packaging come
after the rootfs is reproducible.

```text
manifests -> rootfs -> configured GNOME session -> live image -> installer
```

Run local validation:

```sh
make workstation-check
```

From Windows during the seed phase:

```powershell
mingw32-make workstation-run-seed
```

`workstation-run-seed` direct-boots the live kernel/initrd with WHPX hardware
acceleration and `qemu64`, then launches QEMU through
`tools/run_workstation_seed.ps1`. The make command returns after opening the VM;
close the QEMU window to stop it. It skips Plymouth in the default path because
the Windows GTK display frontend can hang on the splash animation. The splash
path remains available for focused testing:

```powershell
mingw32-make workstation-run-seed-splash
```

For a persistent development VM, create the live-boot persistence disk once and
then use the persistent runner:

```powershell
mingw32-make workstation-vm-persistence-wsl
mingw32-make workstation-run-seed-persistent
```

The disk is `build/workstation/zenithos-persistence.raw`, defaults to a sparse
12GB ext4 filesystem labeled `persistence`, and contains live-boot's
`/persistence.conf` with `/ union`. It is for the dev VM only; final hardware
installs should use the installer and a real partition layout.

The live image should autologin to the `zenith` user and show the Zenith-owned
greeter. If a display-manager fallback ever asks for credentials, use:

```text
user: zenith
password: zenith
```

## Hardware Profiles

ZenithOS boots one image across the development VM and physical laptops. The
`zenith-hardware-detect.service` chooses a profile in this order:

1. Kernel override: `zenith.profile=<id>`.
2. Installed override: `/etc/zenith/hardware-profile`.
3. Automatic DMI, PCI, and CPU detection.
4. Fallback: `generic-workstation`.

Current profiles are `dev-vm`, `lenovo-v14-ada`, and
`generic-workstation`. The Windows/QEMU seed runner passes
`zenith.profile=dev-vm` so VM boot behavior is deterministic. A physical ISO
boot does not need that argument; Lenovo V14-ADA class AMD hardware is detected
automatically, and unknown machines fall back to conservative workstation
defaults.

The active decision is written to:

```text
/run/zenith/hardware-profile
/run/zenith/hardware-profile.env
```

Zenith Settings shows the selected profile under System, Performance, and
Diagnostics.

## Current Desktop Features

- ZenithShell launcher, dock, and quick actions for updates, boot logs, build
  checks, storage audits, running-window switching, system controls, and
  hardware-profile status.
- Zenith Welcome with desktop readiness, boot health, and self-host status.
- Zenith Settings panels for system, appearance, display, sound, network,
  storage, performance, accessibility, build, and diagnostics, with direct
  controls for theme, cursor size, Wi-Fi radio, audio volume, power mode,
  hardware profile override, and persistence diagnostics.
- Zenith Packages with apt search, update inspection, package updates, Flatpak
  updates, and cache cleanup actions.
- Zenith Files with places, search, hidden-file toggle, metadata, parent
  navigation, file opening, and terminal-here.
- Zenith Installer readiness dashboard with non-destructive disk validation,
  Btrfs layout preview, bootloader plan, and install-report actions.
- Zenith Terminal with command sessions and shortcuts for boot logs, updates,
  and build checks.

Inside ZenithOS itself, use:

```sh
zenith-build check
zenith-build rootfs
zenith-build iso
```
