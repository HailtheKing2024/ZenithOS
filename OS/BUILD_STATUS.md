# ZenithOS Build Status

**Current Status:** Beta 2 Preview — All files committed, CI active

---

## Quick Reference

| Command | Description |
|---------|-------------|
| `make workstation-check` | Validate workstation files exist |
| `bash workstation/qa/runtime-check.sh` | Full runtime QA check |
| `make workstation-runtime-check-wsl` | Linux rootfs QA (WSL) |

## Build Pipeline

```
workstation-packages → workstation-repo → workstation-rootfs → workstation-seed-iso
```

## Current State

- **ISO:** `build/workstation/zenithos-seed.iso` (~1.16 GB)
- **SHA256:** Generated at compose time
- **Boot:** UEFI → GRUB → SDDM (pixel-skyscrapers) → KDE Plasma Wayland
- **Install:** Guarded blank-disk with typed erase confirmation
- **Validation:** QEMU smoke tests pass for installed disk + manual login

## Known Issues

See `OS/workstation/release/KNOWN_ISSUES.md` for current known issues.

## Release Notes

See `OS/workstation/release/RELEASE_NOTES.md` for latest release details.

## Hardware Target

Lenovo V14-ADA (AMD) — physical validation pending

---

*Last updated: dynamically via CI*
