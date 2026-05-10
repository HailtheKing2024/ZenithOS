# ZenithOS Release Engineering Pipeline

## Distro Model

ZenithOS is package-first. The image composer should not install random files
directly into the root filesystem except for temporary bootstrap state. Every
owned component becomes a package, packages go into an apt repository, and the
image is composed from that repository.

## Pipeline

1. Validate source tree.
2. Build Zenith-owned packages.
3. Publish packages to a local apt repository.
4. Bootstrap a root filesystem from upstream base packages.
5. Add the Zenith apt repository.
6. Install Zenith packages through apt.
7. Compose the live ISO.
8. Boot-test the ISO.

## Package Ownership

| Package | Owns |
| --- | --- |
| `zenith-settings` | Settings GTK app |
| `zenith-terminal` | Terminal GTK/VTE app |
| `zenith-packages` | apt/Flatpak management UI |
| `zenith-files` | GTK/GIO file manager |
| `zenith-shell-extension` | GNOME Shell integration layer |
| `zenith-workstation-defaults` | os-release, GDM, dconf, sudo, systemd defaults |
| `zenith-builder` | self-hosted `zenith-build` tooling |

## Next Control Step

The next major independence step is to add source package recipes for GNOME 50
modules and build them into `zenith-gnome-*` packages. That gives ZenithOS
control over the platform while preserving apt-based update semantics.
