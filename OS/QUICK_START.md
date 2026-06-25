# ZenithOS Quick Start

## Build The Seed ISO

From PowerShell on the Windows host:

```powershell
cd C:\Users\krish\ZenithOS\OS
mingw32-make workstation-seed-iso-wsl
```

Run the live seed VM:

```powershell
mingw32-make workstation-run-seed
```

Login at SDDM:

```text
user: hailtheking
password: 2976880801
```

## Blank-Disk Install VM

Create a separate blank qcow2 install target. This is not the persistence disk.

```powershell
mingw32-make workstation-vm-install-disk
mingw32-make workstation-run-seed-install-target
```

Inside ZenithOS, open Zenith Installer or run the backend directly:

```sh
lsblk -o NAME,PATH,SIZE,TYPE,FSTYPE,MODEL,MOUNTPOINTS
sudo zenith-install-to-disk --target /dev/DISK --dry-run
sudo zenith-install-to-disk --target /dev/DISK
```

The real install shows a token like:

```text
ZENITH-ERASE-vda
```

Type that exact token only after verifying the target disk is the blank install
disk. The installer partitions the disk, creates the EFI system partition,
creates Btrfs subvolumes, copies the live system, writes `/etc/fstab`, and
installs GRUB's removable EFI fallback.

After installation, close the live VM and boot only the installed disk:

```powershell
mingw32-make workstation-run-installed-disk
```

For automated VM proof, use the gated autoinstall mode instead of clicking the
GUI:

```powershell
mingw32-make workstation-autoinstall-disk
mingw32-make workstation-smoke-installed-disk
```

`workstation-autoinstall-disk` passes `zenith.autoinstall_target=/dev/vda` to
the live system and only attaches the qcow2 install target as a virtio disk.

## Smoke Checks

Live seed smoke:

```powershell
mingw32-make workstation-smoke-seed
```

Installed-disk smoke is available after installing with serial console enabled:

```sh
sudo zenith-install-to-disk --target /dev/DISK --serial-console
```

Then on the host:

```powershell
mingw32-make workstation-smoke-installed-disk
```

This installed-disk smoke boots firmware plus the installed disk only. It does
not use `-cdrom`, `-kernel`, `-initrd`, or live boot arguments.

## Development Persistence

Persistence is only for the live development VM:

```powershell
mingw32-make workstation-vm-persistence-wsl
mingw32-make workstation-run-seed-persistent
```

Do not use `zenithos-persistence.raw` as an install target.
