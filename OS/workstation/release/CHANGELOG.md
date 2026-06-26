# ZenithOS Changelog

## Beta 2 Preview

- Added installer disk selection with non-destructive target validation.
- Added selected-disk dry-run install plan generation for EFI plus Btrfs layout.
- Added ISO SHA256 generation to every live ISO compose.
- Added TUI erase-confirmation prompt in the installer.
- Added Btrfs snapshot infrastructure (snapper config, `zenith-boot-snapshot`, first-boot service).
- Added branded release ISO build target (`make workstation-release-iso`).
- Added GitHub Actions CI workflow for PR validation.
- Added Lenovo V14-ADA hardware probe script and validation checklist.
- Added release notes, known issues, and version stamping in `/etc/os-release`.
- Added QEMU smoke-test tooling for unattended boot checks.

## Beta 1

- Added the Workstation Linux/GNOME track with Zenith-owned shell, apps,
  defaults, and package pipeline.
- Added ZenithShell launcher, dock, quick actions, running-window overview, and
  system controls.
- Added Zenith Settings, Terminal, Files, Packages, Welcome, and Installer.
- Added hardware profiles for `dev-vm`, `lenovo-v14-ada`, and
  `generic-workstation`.
- Added persistent dev VM disk support for live-boot persistence.
- Preserved the Kernel Lab prototype for low-level OS development.
