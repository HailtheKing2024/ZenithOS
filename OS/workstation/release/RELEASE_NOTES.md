# ZenithOS Beta 2 Preview Release Notes

ZenithOS Beta 2 Preview focuses on making the seed ISO behave more like a real
installable workstation while keeping destructive install operations behind
explicit disk selection and typed erase confirmation.

Highlights:

- Installer target disk selection with recursive safety checks.
- Guarded `zenith-install-to-disk` backend for blank-disk installs.
- EFI plus Btrfs layout with `@`, `@home`, `@var`, and `@snapshots`.
- `/etc/fstab` generation, GRUB EFI removable fallback, and optional serial
  console mode for installed-disk smoke tests.
- Installed-disk QEMU smoke proof now verifies a real KDE session, not only
  SDDM: Plasma, KWin, the Zenith panel, icon tasks, wallpaper, and first-party
  desktop launchers must report `PASS`.
- Normal-login QEMU proof now exercises SDDM password entry and verifies the
  installed KDE session without enabling `zenith.desktop_smoke=1` autologin.
- First-boot Flatpak app setup now records deferred network state so Zenith
  Packages can show retry/status instead of leaving only raw boot-log errors.
- ISO checksum generation at compose time.
- `/etc/os-release` version stamping for Beta 2.
- QEMU smoke-test helper for unattended boot checks.

Validation targets:

```sh
make workstation-check
bash workstation/qa/runtime-check.sh
```

Windows persistent VM test:

```powershell
mingw32-make workstation-run-seed-persistent
```

Windows blank-disk install VM flow:

```powershell
mingw32-make workstation-vm-install-disk
mingw32-make workstation-autoinstall-disk
mingw32-make workstation-smoke-installed-disk
```

Windows normal-login VM proof:

```powershell
mingw32-make workstation-vm-install-disk
mingw32-make workstation-autoinstall-manual-login-disk
mingw32-make workstation-smoke-installed-manual-login
```

Manual installed-disk run:

```powershell
mingw32-make workstation-run-installed-disk
```
