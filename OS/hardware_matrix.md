# ZenithOS Hardware Matrix

Target: Lenovo V14-ADA class laptop first, then generic x86_64 laptops.

Runtime profile IDs:

- `dev-vm`: QEMU/KVM/WHPX/Hyper-V/VMware/VirtualBox development hardware.
- `lenovo-v14-ada`: Lenovo V14-ADA class AMD laptop target.
- `generic-workstation`: conservative fallback for unknown x86_64 hardware.

| Area | Expected Device | Linux Interface | Workstation Status | Notes |
| --- | --- | --- | --- | --- |
| Firmware | UEFI x86_64 | EFI, ACPI | Planned | Release images should boot through GRUB or systemd-boot. |
| CPU | AMD Athlon Gold or Ryzen Mobile | Linux x86_64, cpufreq, amd-pstate where available | Planned | Let Linux handle CPU bring-up and power states. |
| Memory | 4GB-8GB DDR4 | Linux MM, zram optional | Planned | 30GB disk target matters more than RAM footprint, but shell must stay light. |
| Graphics | Integrated Radeon Vega | amdgpu, DRM/KMS, Mesa/RADV | Planned | Product desktop uses Wayland/Mutter, not Kernel Lab GOP drawing. |
| Display | Internal laptop panel | DRM connector, Mutter | Planned | Scaling defaults should be tested at 1366x768 and 1920x1080. |
| Keyboard | Internal laptop keyboard | evdev/libinput | Planned | Shell shortcuts belong in ZenithShell defaults. |
| Touchpad | Synaptics or ELAN variant | I2C HID, libinput | Planned | Use libinput acceleration and GNOME settings schema integration. |
| Storage | NVMe or SATA variant | Linux block layer, udisks2 | Planned | Btrfs preferred for rollback snapshots. |
| Wi-Fi | Realtek or MediaTek variant | kernel driver, firmware, NetworkManager, iwd/wpa_supplicant | Research | Probe the actual laptop before finalizing firmware package pins. |
| Audio | AMD/Realtek HDA path | ALSA, PipeWire, WirePlumber | Planned | Zenith Settings should talk to PipeWire policy, not raw ALSA. |
| Bluetooth | Variant dependent | BlueZ, PipeWire Bluetooth audio | Optional | Include only if storage budget permits. |
| Battery/Power | ACPI battery/buttons | upower, logind, power-profiles-daemon | Planned | Power mode switch belongs in quick settings and Settings. |
| Camera | USB camera variant | v4l2, PipeWire portal | Optional | Keep sandbox access portal-mediated. |
| Accessibility | Display/audio/input stack | AT-SPI2, Orca, xdg portals | Planned | Accessibility is part of desktop parity, not an afterthought. |

## Bring-Up Priority

1. Bootloader, Linux kernel, firmware, and rootfs.
2. systemd, udev, dbus, logind, journald, and polkit.
3. GDM, Wayland, Mutter, GNOME Shell internals.
4. GTK/libadwaita runtime and ZenithShell extension.
5. NetworkManager, PipeWire, Flatpak, apt update flow.
6. Zenith Settings, Terminal, Packages, Files, and Installer.
