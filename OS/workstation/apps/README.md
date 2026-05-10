# Zenith Apps

First-party apps should use GTK4/libadwaita and normal Linux desktop services.
The goal is a coherent Zenith desktop, not a separate incompatible app stack.

## Planned Apps

- Zenith Settings: control center for network, sound, display, power, packages,
  accounts, and system information.
- Zenith Terminal: VTE-based terminal with sensible defaults.
- Zenith Packages: apt plus Flatpak package/update front end.
- Zenith Files: GIO/GVfs file manager.
- Zenith Installer: live image installer and recovery entry point.

## Current Scaffolds

- `zenith-settings/`
- `zenith-terminal/`
- `zenith-packages/`
- `zenith-files/`

The rootfs builder installs their Python modules under `/usr/share/zenith/apps`,
their launchers under `/usr/bin`, and their desktop entries under
`/usr/share/applications`.
