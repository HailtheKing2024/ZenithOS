# ZenithShell Specification

## Purpose

ZenithShell provides a modern, fast, and clean desktop surface using KDE Plasma internals. It is designed to feel cohesive, visually premium, and highly responsive under virtualized hardware profiles.

## Feature Baseline

### Panel (Bottom)

- left: Kickoff Application Launcher (standard menu widget).
- center: Icons-Only Task Manager pinning the default first-party applications.
- right: System Tray (network/Wi-Fi, audio/PipeWire, battery/power) and Digital Clock.
- theme: Breeze Dark default.

### Application Menu (Kickoff)

Default first-party apps pinned to favorites:

- Zenith Welcome
- Zenith Terminal
- Zenith Settings
- Zenith Files
- Zenith Packages
- Zenith Builder

### System Tray

Expose:
- Wi-Fi (via plasma-nm / NetworkManager integration)
- Audio output & input volume (via plasma-pa / WirePlumber integration)
- Battery charging state and system power profile policy

## Interaction

- Super key opens the Kickoff menu.
- Type-to-search is supported inside Kickoff.
- Click app icons on the Panel or Kickoff to launch.
- Settings and Packages use polkit-kde-agent-1 for authorization prompts.

## Visual Direction

- dark mode by default (Breeze Dark basis)
- 8px border radius for normal windows and widgets
- high visibility Breeze pointer (size 48) for development VM environments
- restrained window animations to minimize virtual display lag

## Implementation Strategy

- **Milestone 1**: Custom desktop layout configurations (`plasma-org.kde.plasma.desktop-appletsrc`), `kdeglobals`, `kwinrc`, and `kcminputrc` packaged in `zenith-plasma-config`.
- **Milestone 2**: Custom Look-and-Feel theme integration and custom wallpaper packaging.
- **Milestone 3**: Zenith shell widgets and custom panel layout scripts.
