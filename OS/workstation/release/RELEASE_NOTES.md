# ZenithOS Beta 2 Preview Release Notes

ZenithOS Beta 2 Preview focuses on making the seed ISO behave more like a real
installable workstation while keeping destructive install operations locked.

Highlights:

- Installer target disk selection with safety checks.
- EFI plus Btrfs dry-run install plan generation.
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
