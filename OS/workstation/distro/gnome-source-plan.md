# Source-Built GNOME Platform Plan

ZenithOS should eventually own its GNOME platform packages instead of taking
them prebuilt from another distribution.

## Target

Build GNOME 50 platform components from source into Zenith-owned Debian
packages:

```text
GNOME upstream source
-> Zenith source package recipe
-> clean build chroot
-> zenith-gnome-* .deb
-> Zenith apt repository
-> ZenithOS compose
```

## Package Families

Start with the shell-visible platform:

- `zenith-gnome-shell`
- `zenith-mutter`
- `zenith-gsettings-desktop-schemas`
- `zenith-gnome-session`
- `zenith-gnome-settings-daemon`
- `zenith-gnome-control-center`
- `zenith-libadwaita`
- `zenith-gtk4`
- `zenith-vte`
- `zenith-xdg-desktop-portal-gnome`

## Build Rules

- Build in clean chroots.
- Never install GNOME directly from `meson install` into the final image.
- Package all outputs.
- Publish all outputs to a signed apt repository.
- Compose images only from repositories.
- Keep a recovery session available while ZenithShell changes are experimental.

## Bootstrap Reality

The first Workstation seed can still use upstream binary packages for compiler,
kernel, systemd, and emergency GNOME runtime coverage. The long-term control
track replaces those GNOME binaries with Zenith-built packages one package
family at a time.
