# ZenithShell

ZenithShell is the visible desktop experience for ZenithOS Workstation.

The first implementation is a GNOME Shell integration layer:

- keep Mutter/Wayland and GNOME session internals
- apply Zenith top-bar identity
- tune overview defaults
- expose a modern app grid and dash direction
- keep stock GNOME Shell available as a recovery session

Later milestones can replace larger shell surfaces while retaining GNOME
interfaces underneath.

## UI Shape

- top bar with Activities, centered Zenith identity, and system status
- overview-first window and app launching
- search-first launcher
- clean Adwaita-adjacent surfaces
- subtle accent color, restrained shadows, readable spacing
- no Windows-style taskbar

## Files

- `shell-spec.md`: product behavior and UX architecture
- `gnome-extension/`: first ZenithShell integration point
