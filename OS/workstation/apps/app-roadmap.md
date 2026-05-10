# Zenith App Roadmap

## Feature Order

Build the daily desktop first:

1. Settings
2. Terminal
3. Packages
4. Files
5. Installer

Installer work stays last until the desktop and system tools feel usable.

## Zenith Settings

Milestone 1:

- System overview.
- Network, Sound, Display, Power, Packages, and About panels.
- Clear status rows for services that are not wired yet.

Milestone 2:

- NetworkManager dbus integration.
- PipeWire/WirePlumber status.
- power-profiles-daemon and upower status.
- GNOME schema writes for desktop preferences.

## Zenith Terminal

Milestone 1:

- GTK/libadwaita shell.
- command area placeholder.
- profile/status UI.
- VTE runtime detection.

Milestone 2:

- VTE terminal widget.
- copy/paste.
- tabs.
- profile settings.

## Zenith Packages

Milestone 1:

- apt and Flatpak status panels.
- update/install/remove action placeholders.
- storage budget warnings.

Milestone 2:

- apt update/install/remove worker.
- Flatpak search/install/remove worker.
- polkit prompts.
- rollback snapshot prompts.

## Zenith Files

Milestone 1:

- GTK/GIO file list.
- places sidebar.
- simple path navigation.

Milestone 2:

- copy/move/delete.
- mounted volume handling.
- portal-aware file operations.

## Installer

Installer begins after shell, settings, terminal, packages, and files are usable.
