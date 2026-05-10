# ZenithShell Specification

## Purpose

ZenithShell provides a modern GNOME-like desktop surface using GNOME internals.
It should feel clean, fast, and coherent without becoming a theme-only fork.

## Feature Baseline

### Top Bar

- left: Activities.
- center: ZenithOS identity in early builds, clock once shell services are wired.
- right: network, audio, battery, power, and session menu.
- no app taskbar.

### Activities Overview

- search field at the top.
- open windows in the center.
- workspace strip where Mutter exposes it.
- dash for pinned apps.
- app grid button.

### App Grid

Default first-party apps:

- Zenith Settings
- Zenith Terminal
- Zenith Files
- Zenith Packages
- Zenith Builder

Installer is intentionally excluded from the daily app grid until the desktop
feature set is usable.

### System Menu

Expose:

- Wi-Fi
- audio output
- brightness
- power mode
- battery
- lock
- restart/shutdown

## Interaction

- Super opens overview.
- Type-to-search works from the overview.
- Escape exits overview.
- Click app tile to launch.
- Drag windows between workspaces.
- Settings and Packages use polkit for privileged actions.

## Visual Direction

- dark and light mode from day one
- 8px base radius for normal controls
- restrained elevation
- no decorative blobs or oversized marketing layouts
- dense enough for real work, but not cramped

## Implementation Strategy

Milestone 1 uses a GNOME Shell extension to apply Zenith identity and defaults.
Milestone 2 adds a real Zenith quick-settings surface and overview tuning.
Milestone 3 replaces more visible shell surface while keeping Mutter and GNOME
session internals.
