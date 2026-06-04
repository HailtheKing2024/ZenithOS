# ZenithOS Workstation Roadmap

## Phase 0: Repository Pivot

- Preserve Kernel Lab.
- Add Workstation architecture, package manifests, storage budget, shell spec,
  app scaffolds, and validation.
- Keep self-hosting documented, but do not let install/build automation drive
  the product schedule.

## Phase 1: Desktop Feature Baseline

- Build Zenith-owned `.deb` packages for shell, apps, defaults, and builder.
- Publish those packages into a local Zenith apt repository.
- ZenithShell identity layer on KDE Plasma internals.
- Modern bottom panel, kickoff menu, applet shortcuts, and system tray direction.
- Zenith theme tokens and default KDE Plasma settings.
- First-party app launchers and desktop entries.
- Stock Plasma session retained as recovery.

## Phase 2: First-Party Apps

- Zenith Settings with Network, Sound, Display, Power, Packages, and System
  panels.
- Zenith Terminal using GTK/libadwaita now and VTE when the runtime is present.
- Zenith Packages as an apt plus Flatpak front end.
- Zenith Files as a GTK/GIO file manager.
- All app UIs are unprivileged; privileged operations go through polkit/dbus.

## Phase 3: System Integration

- Wire Settings to NetworkManager, PipeWire, power-profiles-daemon, upower, and
  KDE/GTK schemas.
- Wire Packages to apt, Flatpak, storage-budget warnings, and rollback prompts.
- Wire Files to GIO/GVfs and xdg portals.
- Add notifications, error states, loading states, and offline behavior.

## Phase 4: Hardware Polish

- Lenovo V14-ADA touchpad tuning.
- Power profile defaults.
- Wi-Fi firmware verification.
- AMDGPU/Mesa validation.
- Suspend/resume and lid behavior testing.

## Phase 5: Image, Installer, And Self-Hosting

- Build live image.
- Boot live image and rebuild rootfs from inside ZenithOS.
- Promote source-built KDE/Plasma packages into the Zenith apt repository.
- Add graphical installer.
- Add Btrfs snapshot rollback.
- Add first-boot user creation and update checks.
