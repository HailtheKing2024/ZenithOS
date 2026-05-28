# ZenithOS Known Issues

## Beta 2 Preview

- The installer is still non-destructive. It validates target disks and prints a
  dry-run plan, but it does not write partitions yet.
- The Zenith greeter is a first-session Zenith overlay on top of GDM autologin;
  a full custom pre-auth login manager is not implemented yet.
- The Windows QEMU GTK frontend can still hang with Plymouth animation enabled,
  so the default runner uses the direct kernel/initrd path with Plymouth off.
- The persistent VM disk is a development artifact and may occupy its full 12GB
  allocation on Windows-mounted filesystems.
- GNOME Shell extension syntax is validated indirectly through source/runtime
  checks in the bootstrap environment; full extension validation requires
  booting the graphical session.
- Physical Lenovo V14-ADA hardware validation is still pending on the target
  machine.
