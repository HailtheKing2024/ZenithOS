# ZenithOS Known Issues

## Beta 2 Preview

- Installed-disk smoke passed in QEMU: the blank qcow2 install boots from its
  own EFI fallback, `/run/live` is absent, SDDM starts, KDE launches, and the
  session report verifies Plasma, KWin, panel, wallpaper, and first-party app
  launchers. The normal SDDM password-entry path also passes in QEMU via
  `workstation-smoke-installed-manual-login`.
- The Zenith greeter is a first-session Zenith overlay on top of SDDM; the
  pixel-skyscrapers SDDM theme is installed, but the full pre-auth flow still
  needs physical display validation.
- The Windows QEMU GTK frontend can still hang with Plymouth animation enabled,
  so the default runner uses the direct kernel/initrd path with Plymouth off.
- The persistent VM disk is a development artifact and may occupy its full 12GB
  allocation on Windows-mounted filesystems.
- First-boot Flatpak app installation is network-dependent. If Flathub DNS or
  network access is unavailable, setup is deferred and surfaced in Zenith
  Packages for retry.
- Physical Lenovo V14-ADA hardware validation is still pending on the target
  machine, including display, touchpad, Wi-Fi, power profile, suspend/resume,
  and bootloader behavior.
- `zenith-boot-snapshot` utility and snapper first-boot service are committed
  but await end-to-end smoke validation in a real install.
- CI workflow (`workstation-check` + `runtime-check`) is configured but has not
  yet been triggered via a GitHub PR against the repository.
