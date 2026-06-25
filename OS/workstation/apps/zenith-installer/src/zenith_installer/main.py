import json
import os
import shlex
import shutil
import subprocess

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gio, Gtk


MIN_INSTALL_BYTES = 30 * 1024 * 1024 * 1024
LIVE_MOUNTPOINTS = (
    "/run/live/medium",
    "/run/live/persistence",
    "/run/live/rootfs",
)


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
        [
            "lsblk",
            "--bytes",
            "--json",
            "--output",
            "NAME,PATH,PKNAME,SIZE,TYPE,MODEL,MOUNTPOINTS,FSTYPE,TRAN,RM,RO,HOTPLUG,LABEL,UUID,SERIAL",
        ],
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


def flatten_block_tree(device):
    yield device
    for child in device.get("children") or []:
        yield from flatten_block_tree(child)


def boolish(value):
    if isinstance(value, bool):
        return value
    if isinstance(value, int):
        return value != 0
    if value is None:
        return False
    return str(value).strip().lower() in {"1", "true", "yes", "on"}


def parse_size_bytes(value):
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def child_mountpoints(device):
    mountpoints = []
    for item in flatten_block_tree(device):
        mountpoints.extend(point for point in item.get("mountpoints") or [] if point)
    return mountpoints


def live_media_sources():
    sources = []
    for mountpoint in LIVE_MOUNTPOINTS:
        source = command_output(["findmnt", "-rn", "-o", "SOURCE", mountpoint], timeout=2)
        if source:
            sources.append(first_line(source, ""))
    return sources


def live_media_disks(devices):
    sources = live_media_sources()
    disks = set()
    for device in devices:
        disk_path = device_path(device)
        node_paths = [device_path(item) for item in flatten_block_tree(device)]
        for source in sources:
            if any(source == path or source.startswith(path) for path in node_paths):
                disks.add(disk_path)
    return disks


def format_size(size):
    size = parse_size_bytes(size)
    if size is None:
        return "unknown size"
    gib = size / 1024 / 1024 / 1024
    return f"{gib:.1f} GiB"


def disk_issues(device, devices=None):
    reasons = []
    devices = devices or [device]
    nodes = list(flatten_block_tree(device))
    size = parse_size_bytes(device.get("size"))
    mountpoints = child_mountpoints(device)
    path = device_path(device)
    if device.get("type") != "disk":
        reasons.append("not a whole disk")
    if size is None:
        reasons.append("unknown size")
    elif size < MIN_INSTALL_BYTES:
        reasons.append("below 30GB target")
    if any(boolish(item.get("ro")) for item in nodes):
        reasons.append("read-only")
    if any(boolish(item.get("rm")) or boolish(item.get("hotplug")) for item in nodes):
        reasons.append("removable or hotplug")
    if any((item.get("tran") or "").lower() == "usb" for item in nodes):
        reasons.append("USB target locked")
    if mountpoints:
        reasons.append("mounted child")
    if path in live_media_disks(devices):
        reasons.append("current live media")
    if path in ["/dev/sr0", "/dev/fd0"] or path.startswith(("/dev/loop", "/dev/ram", "/dev/zram")):
        reasons.append("not an install target")
    return reasons


def disk_validation(device, devices=None):
    reasons = disk_issues(device, devices)
    if not reasons:
        return "Candidate: passes guarded install checks"
    return "Locked: " + ", ".join(reasons)


def partition_path(disk, number):
    if disk.startswith("/dev/nvme") or disk.startswith("/dev/mmcblk") or disk.startswith("/dev/loop"):
        return f"{disk}p{number}"
    return f"{disk}{number}"


def install_plan_for_disk(disk):
    return "\n".join([
        "# ZenithOS installer dry-run",
        "# This prints the backend plan; it does not write to disk.",
        f"sudo zenith-install-to-disk --target {shlex.quote(disk)} --dry-run",
        "",
        "# Real install is separate and requires the backend confirmation token:",
        f"# sudo zenith-install-to-disk --target {shlex.quote(disk)}",
    ])


class InstallerWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="Zenith Installer")
        self.set_default_size(900, 640)
        self.devices = lsblk_devices()
        self.selected_disk = None
        self.disk_explicitly_selected = False
        for device in self.devices:
            if not disk_issues(device, self.devices):
                self.selected_disk = device_path(device)
                break

        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title="Installer", subtitle="Guarded install mode"))
        toolbar.add_top_bar(header)
        toolbar.set_content(self._build_content())
        self.set_content(toolbar)

    def _build_content(self):
        scroller = Gtk.ScrolledWindow()
        page = Adw.PreferencesPage()
        page.set_title("Install")
        scroller.set_child(page)

        gate = Adw.PreferencesGroup(title="Install Mode")
        gate.add(Adw.ActionRow(title="Current State", subtitle="Real install requires explicit disk selection and typed erase confirmation"))
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
        for command in [
            "zenith-install-to-disk",
            "parted",
            "mkfs.vfat",
            "mkfs.btrfs",
            "btrfs",
            "rsync",
            "grub-install",
            "update-initramfs",
            "update-grub",
            "chroot",
            "mount",
            "umount",
        ]:
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
                disk_validation(device, self.devices),
            ] if part)
            row = Adw.ActionRow(title=title, subtitle=subtitle)
            button = Gtk.Button(label="Select")
            button.set_sensitive(not disk_issues(device, self.devices))
            button.connect("clicked", self._select_disk, title)
            row.add_suffix(button)
            row.set_activatable_widget(button)
            disks.add(row)
        page.add(disks)

        selected = Adw.PreferencesGroup(title="Selected Target")
        if self.selected_disk:
            if self.disk_explicitly_selected:
                subtitle = "Ready for guarded install; backend will require the erase token"
            else:
                subtitle = "Auto-detected for dry-run only; click Select before real install"
            selected.add(Adw.ActionRow(title=self.selected_disk, subtitle=subtitle))
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
        can_install = self.disk_explicitly_selected and self._selected_eligible_disk() is not None
        row = Adw.ActionRow(
            title="Erase disk and install ZenithOS",
            subtitle="Opens a root installer that requires the backend erase token before writing",
        )
        button = Gtk.Button(label="Install...")
        button.set_sensitive(can_install)
        button.connect("clicked", self._launch_install)
        row.add_suffix(button)
        row.set_activatable_widget(button)
        actions.add(row)
        page.add(actions)

        return scroller

    def _select_disk(self, _button, disk):
        self.selected_disk = disk
        self.disk_explicitly_selected = True
        self.set_content(None)
        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title="Installer", subtitle=f"Selected {disk}"))
        toolbar.add_top_bar(header)
        toolbar.set_content(self._build_content())
        self.set_content(toolbar)

    def _selected_eligible_disk(self):
        if not self.selected_disk:
            return None
        devices = lsblk_devices()
        for device in devices:
            if device_path(device) == self.selected_disk and not disk_issues(device, devices):
                return self.selected_disk
        return None

    def _dry_run_command(self):
        disk = self._selected_eligible_disk()
        if not disk:
            return "printf '%s\\n' 'No eligible install target selected.'; exec bash"
        return f"sudo zenith-install-to-disk --target {shlex.quote(disk)} --dry-run; exec bash"

    def _install_command(self):
        disk = self._selected_eligible_disk()
        if not disk:
            return "printf '%s\\n' 'Selected install target is no longer eligible.'; exec bash"
        return f"sudo zenith-install-to-disk --target {shlex.quote(disk)}; exec bash"

    def _launch_install(self, button):
        self._launch(button, ["zenith-terminal", "--command", self._install_command()])

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
