import json
import os
import shutil
import subprocess

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gio, Gtk


def command_available(command):
    if shutil.which(command) is not None:
        return True
    return any(os.path.exists(os.path.join(path, command)) for path in ["/sbin", "/usr/sbin"])


def command_output(command, timeout=4):
    try:
        result = subprocess.run(
            command,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired):
        return ""
    return result.stdout.strip()


def first_line(text, fallback="Unavailable"):
    return text.splitlines()[0] if text else fallback


def read_profile():
    try:
        with open("/run/zenith/hardware-profile", "r", encoding="utf-8") as handle:
            return handle.read().strip() or "unknown"
    except OSError:
        return "unknown"


def live_persistence_status():
    cmdline = command_output(["cat", "/proc/cmdline"])
    if "persistence" not in cmdline:
        return "Not requested"
    if os.path.exists("/run/live/persistence"):
        return "Requested and detected"
    return "Requested; live-boot has not mounted persistence"


def lsblk_devices():
    if not command_available("lsblk"):
        return []
    result = subprocess.run(
        ["lsblk", "--bytes", "--json", "--output", "NAME,PATH,SIZE,TYPE,MODEL,MOUNTPOINTS,FSTYPE,TRAN,RM,RO"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        timeout=4,
    )
    try:
        data = json.loads(result.stdout)
    except json.JSONDecodeError:
        return []
    return [item for item in data.get("blockdevices", []) if item.get("type") == "disk"]


def format_size(size):
    try:
        size = int(size)
    except (TypeError, ValueError):
        return "unknown size"
    gib = size / 1024 / 1024 / 1024
    return f"{gib:.1f} GiB"


def disk_validation(device):
    reasons = []
    size = int(device.get("size") or 0)
    mountpoints = [item for item in device.get("mountpoints") or [] if item]
    if size < 30 * 1024 * 1024 * 1024:
        reasons.append("below 30GB target")
    if device.get("ro"):
        reasons.append("read-only")
    if device.get("rm"):
        reasons.append("removable")
    if mountpoints:
        reasons.append("mounted")
    if not reasons:
        return "Candidate: passes non-destructive readiness checks"
    return "Locked: " + ", ".join(reasons)


class InstallerWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="Zenith Installer")
        self.set_default_size(900, 640)

        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title="Installer", subtitle="Preview mode"))
        toolbar.add_top_bar(header)
        toolbar.set_content(self._build_content())
        self.set_content(toolbar)

    def _build_content(self):
        scroller = Gtk.ScrolledWindow()
        page = Adw.PreferencesPage()
        page.set_title("Install")
        scroller.set_child(page)

        gate = Adw.PreferencesGroup(title="Install Mode")
        gate.add(Adw.ActionRow(title="Current State", subtitle="Preview only: no partitioning or disk writes are enabled"))
        gate.add(Adw.ActionRow(title="Firmware", subtitle="UEFI detected" if os.path.exists("/sys/firmware/efi") else "Legacy boot or VM direct kernel path"))
        gate.add(Adw.ActionRow(title="Hardware profile", subtitle=read_profile()))
        gate.add(Adw.ActionRow(title="Live persistence", subtitle=live_persistence_status()))
        gate.add(Adw.ActionRow(title="Package source", subtitle="Zenith local apt repository plus Debian Trixie base packages"))
        page.add(gate)

        layout = Adw.PreferencesGroup(title="Target Layout")
        layout.add(Adw.ActionRow(title="EFI system partition", subtitle="512MiB FAT32 mounted at /boot/efi"))
        layout.add(Adw.ActionRow(title="Root filesystem", subtitle="Btrfs with @, @home, @var, @snapshots, and rollback-ready layout"))
        layout.add(Adw.ActionRow(title="Swap policy", subtitle="zram first; no mandatory swap partition in the 30GB target"))
        layout.add(Adw.ActionRow(title="Bootloader", subtitle="GRUB UEFI for the first installer milestone; systemd-boot remains a future option"))
        page.add(layout)

        tools = Adw.PreferencesGroup(title="Required Tools")
        for command in ["parted", "sgdisk", "mkfs.btrfs", "grub-install", "efibootmgr", "rsync", "update-initramfs"]:
            tools.add(Adw.ActionRow(
                title=command,
                subtitle="Available" if command_available(command) else "Missing",
            ))
        page.add(tools)

        disks = Adw.PreferencesGroup(title="Detected Disks")
        devices = lsblk_devices()
        if not devices:
            disks.add(Adw.ActionRow(title="No disks listed", subtitle="lsblk did not report installable disks"))
        for device in devices:
            title = device.get("path") or f"/dev/{device.get('name', '?')}"
            subtitle = " | ".join(part for part in [
                format_size(device.get("size")),
                device.get("model", ""),
                device.get("tran", ""),
                disk_validation(device),
            ] if part)
            disks.add(Adw.ActionRow(title=title, subtitle=subtitle))
        page.add(disks)

        actions = Adw.PreferencesGroup(title="Actions")
        actions.add(self._terminal_row(
            "Install readiness report",
            "Show firmware, profile, disks, filesystems, and required tools",
            "echo 'ZenithOS install readiness'; echo; cat /proc/cmdline; echo; cat /run/zenith/hardware-profile.env 2>/dev/null || true; echo; lsblk -o NAME,PATH,SIZE,TYPE,FSTYPE,MODEL,MOUNTPOINTS; echo; for c in parted sgdisk mkfs.btrfs grub-install efibootmgr rsync update-initramfs; do command -v $c >/dev/null && echo OK $c || echo MISSING $c; done; exec bash",
        ))
        actions.add(self._terminal_row(
            "Btrfs layout preview",
            "Print the planned partition and subvolume commands without running them",
            "cat <<'PLAN'\nparted TARGET --script mklabel gpt\nparted TARGET --script mkpart ESP fat32 1MiB 513MiB\nparted TARGET --script set 1 esp on\nparted TARGET --script mkpart zenith-root btrfs 513MiB 100%\nmkfs.vfat -F32 TARGET1\nmkfs.btrfs -L ZenithOS TARGET2\nmount TARGET2 /mnt\nbtrfs subvolume create /mnt/@\nbtrfs subvolume create /mnt/@home\nbtrfs subvolume create /mnt/@var\nbtrfs subvolume create /mnt/@snapshots\nPLAN\nexec bash",
        ))
        row = Adw.ActionRow(
            title="Install to disk",
            subtitle="Locked until destructive install policy, confirmation UI, and rollback plan are implemented",
        )
        button = Gtk.Button(label="Locked")
        button.set_sensitive(False)
        row.add_suffix(button)
        actions.add(row)
        page.add(actions)

        return scroller

    def _terminal_row(self, title, subtitle, command):
        row = Adw.ActionRow(title=title, subtitle=subtitle)
        button = Gtk.Button(label="Open")
        button.connect("clicked", self._launch, ["zenith-terminal", "--command", command])
        row.add_suffix(button)
        row.set_activatable_widget(button)
        return row

    def _launch(self, _button, command):
        try:
            subprocess.Popen(command)
        except OSError:
            pass


class InstallerApp(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id="os.zenith.Installer",
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )

    def do_activate(self):
        window = self.props.active_window
        if window is None:
            window = InstallerWindow(self)
        window.present()


def main():
    app = InstallerApp()
    return app.run(None)


if __name__ == "__main__":
    raise SystemExit(main())
