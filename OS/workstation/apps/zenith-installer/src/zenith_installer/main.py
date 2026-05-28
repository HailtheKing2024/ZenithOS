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


def device_path(device):
    return device.get("path") or f"/dev/{device.get('name', '?')}"


def format_size(size):
    try:
        size = int(size)
    except (TypeError, ValueError):
        return "unknown size"
    gib = size / 1024 / 1024 / 1024
    return f"{gib:.1f} GiB"


def disk_issues(device):
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
    path = device_path(device)
    if path in ["/dev/sr0", "/dev/fd0"] or path.startswith("/dev/loop"):
        reasons.append("not an install target")
    return reasons


def disk_validation(device):
    reasons = disk_issues(device)
    if not reasons:
        return "Candidate: passes non-destructive readiness checks"
    return "Locked: " + ", ".join(reasons)


def partition_path(disk, number):
    if disk.startswith("/dev/nvme") or disk.startswith("/dev/mmcblk") or disk.startswith("/dev/loop"):
        return f"{disk}p{number}"
    return f"{disk}{number}"


def install_plan_for_disk(disk):
    efi = partition_path(disk, 1)
    root = partition_path(disk, 2)
    return "\n".join([
        "# ZenithOS installer dry-run plan",
        "# Destructive commands are intentionally printed only.",
        f"TARGET={disk}",
        f"EFI_PARTITION={efi}",
        f"ROOT_PARTITION={root}",
        "",
        "parted \"$TARGET\" --script mklabel gpt",
        "parted \"$TARGET\" --script mkpart ESP fat32 1MiB 513MiB",
        "parted \"$TARGET\" --script set 1 esp on",
        "parted \"$TARGET\" --script mkpart zenith-root btrfs 513MiB 100%",
        "mkfs.vfat -F32 \"$EFI_PARTITION\"",
        "mkfs.btrfs -f -L ZenithOS \"$ROOT_PARTITION\"",
        "mount \"$ROOT_PARTITION\" /mnt",
        "btrfs subvolume create /mnt/@",
        "btrfs subvolume create /mnt/@home",
        "btrfs subvolume create /mnt/@var",
        "btrfs subvolume create /mnt/@snapshots",
        "umount /mnt",
        "mount -o subvol=@,compress=zstd,noatime \"$ROOT_PARTITION\" /mnt",
        "mkdir -p /mnt/{boot/efi,home,var,.snapshots}",
        "mount \"$EFI_PARTITION\" /mnt/boot/efi",
        "mount -o subvol=@home,compress=zstd,noatime \"$ROOT_PARTITION\" /mnt/home",
        "mount -o subvol=@var,compress=zstd,noatime \"$ROOT_PARTITION\" /mnt/var",
        "mount -o subvol=@snapshots,compress=zstd,noatime \"$ROOT_PARTITION\" /mnt/.snapshots",
        "rsync -aAXH --exclude=/dev/* --exclude=/proc/* --exclude=/sys/* --exclude=/run/* --exclude=/tmp/* --exclude=/mnt/* / /mnt/",
        "grub-install --target=x86_64-efi --efi-directory=/mnt/boot/efi --bootloader-id=ZenithOS --recheck",
        "chroot /mnt update-initramfs -u",
        "chroot /mnt update-grub",
    ])


class InstallerWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="Zenith Installer")
        self.set_default_size(900, 640)
        self.devices = lsblk_devices()
        self.selected_disk = None
        for device in self.devices:
            if not disk_issues(device):
                self.selected_disk = device_path(device)
                break

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
        if not self.devices:
            disks.add(Adw.ActionRow(title="No disks listed", subtitle="lsblk did not report installable disks"))
        for device in self.devices:
            title = device_path(device)
            subtitle = " | ".join(part for part in [
                format_size(device.get("size")),
                device.get("model", ""),
                device.get("tran", ""),
                disk_validation(device),
            ] if part)
            row = Adw.ActionRow(title=title, subtitle=subtitle)
            button = Gtk.Button(label="Select")
            button.set_sensitive(not disk_issues(device))
            button.connect("clicked", self._select_disk, title)
            row.add_suffix(button)
            row.set_activatable_widget(button)
            disks.add(row)
        page.add(disks)

        selected = Adw.PreferencesGroup(title="Selected Target")
        if self.selected_disk:
            selected.add(Adw.ActionRow(title=self.selected_disk, subtitle="Ready for dry-run plan generation; destructive install remains locked"))
        else:
            selected.add(Adw.ActionRow(title="No disk selected", subtitle="No detected disk currently passes the non-destructive install target checks"))
        page.add(selected)

        actions = Adw.PreferencesGroup(title="Actions")
        actions.add(self._terminal_row(
            "Install readiness report",
            "Show firmware, profile, disks, filesystems, and required tools",
            "echo 'ZenithOS install readiness'; echo; cat /proc/cmdline; echo; cat /run/zenith/hardware-profile.env 2>/dev/null || true; echo; lsblk -o NAME,PATH,SIZE,TYPE,FSTYPE,MODEL,MOUNTPOINTS; echo; for c in parted sgdisk mkfs.btrfs grub-install efibootmgr rsync update-initramfs; do command -v $c >/dev/null && echo OK $c || echo MISSING $c; done; exec bash",
        ))
        actions.add(self._terminal_row(
            "Dry-run install plan",
            "Print the selected disk's EFI and Btrfs install plan without running it",
            self._dry_run_command(),
        ))
        row = Adw.ActionRow(
            title="Install to disk",
            subtitle="Locked until typed confirmation and final rollback policy are implemented",
        )
        button = Gtk.Button(label="Requires typed confirmation")
        button.set_sensitive(False)
        row.add_suffix(button)
        actions.add(row)
        page.add(actions)

        return scroller

    def _select_disk(self, _button, disk):
        self.selected_disk = disk
        self.set_content(None)
        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title="Installer", subtitle=f"Selected {disk}"))
        toolbar.add_top_bar(header)
        toolbar.set_content(self._build_content())
        self.set_content(toolbar)

    def _dry_run_command(self):
        disk = self.selected_disk or "NO_VALID_TARGET"
        plan = install_plan_for_disk(disk)
        escaped = plan.replace("'", "'\"'\"'")
        return f"printf '%s\\n' '{escaped}'; exec bash"

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
