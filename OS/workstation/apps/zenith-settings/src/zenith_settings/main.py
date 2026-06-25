import os
import platform
import shlex
import shutil
import subprocess

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gio, Gtk


def read_os_release():
    values = {}
    try:
        with open("/etc/os-release", "r", encoding="utf-8") as handle:
            for line in handle:
                if "=" not in line:
                    continue
                key, value = line.rstrip().split("=", 1)
                values[key] = value.strip('"')
    except OSError:
        pass
    return values


def read_profile_env():
    values = {}
    try:
        with open("/run/zenith/hardware-profile.env", "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, raw = line.split("=", 1)
                try:
                    parsed = shlex.split(raw)
                except ValueError:
                    parsed = []
                values[key] = parsed[0] if parsed else raw.strip().strip("'\"")
    except OSError:
        pass

    if "ZENITH_PROFILE_ID" not in values:
        try:
            with open("/run/zenith/hardware-profile", "r", encoding="utf-8") as handle:
                values["ZENITH_PROFILE_ID"] = handle.read().strip()
        except OSError:
            values["ZENITH_PROFILE_ID"] = "unknown"

    values.setdefault("ZENITH_PROFILE_NAME", values["ZENITH_PROFILE_ID"])
    values.setdefault("ZENITH_PROFILE_SOURCE", "unknown")
    values.setdefault("ZENITH_PROFILE_REASON", "Detector has not reported yet")
    values.setdefault("ZENITH_PROFILE_POWER_POLICY", "unknown")
    values.setdefault("ZENITH_PROFILE_GRAPHICS_STACK", "unknown")
    values.setdefault("ZENITH_PROFILE_INPUT_STACK", "unknown")
    return values


def command_status(command):
    return "Available" if shutil.which(command) else "Missing"


def service_status(unit):
    if not shutil.which("systemctl"):
        return "systemctl unavailable"
    result = subprocess.run(
        ["systemctl", "is-active", unit],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    status = result.stdout.strip()
    return status if status else "unknown"


def command_output(command, timeout=3):
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
        return "Unavailable"
    text = result.stdout.strip()
    return text if text else "Unavailable"


def gsettings_bool(schema, key, default=False):
    value = command_output(["gsettings", "get", schema, key])
    if value == "true":
        return True
    if value == "false":
        return False
    return default


def first_line(text):
    return text.splitlines()[0] if text and text != "Unavailable" else text


def memory_summary():
    try:
        with open("/proc/meminfo", "r", encoding="utf-8") as handle:
            values = {}
            for line in handle:
                key, raw = line.split(":", 1)
                values[key] = int(raw.strip().split()[0])
        total_gib = values.get("MemTotal", 0) / 1024 / 1024
        available_gib = values.get("MemAvailable", 0) / 1024 / 1024
        return f"{available_gib:.1f} GiB available of {total_gib:.1f} GiB"
    except (OSError, ValueError, IndexError):
        return "Unavailable"


def disk_summary(path="/"):
    try:
        usage = shutil.disk_usage(path)
    except OSError:
        return "Unavailable"
    free = usage.free / 1024 / 1024 / 1024
    total = usage.total / 1024 / 1024 / 1024
    used = (usage.used / usage.total * 100) if usage.total else 0
    return f"{free:.1f} GiB free of {total:.1f} GiB ({used:.0f}% used)"


def file_status(path):
    return "Present" if os.path.exists(path) else "Missing"


class SettingsWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="Zenith Settings")
        self.set_default_size(980, 680)

        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        header.set_title_widget(Gtk.Label(label="Settings"))
        toolbar.add_top_bar(header)

        split = Adw.NavigationSplitView()
        sidebar_page = Adw.NavigationPage(title="Categories", child=self._build_sidebar())
        self.content_page = Adw.NavigationPage(title="System", child=self._build_content("System"))
        split.set_sidebar(sidebar_page)
        split.set_content(self.content_page)
        toolbar.set_content(split)

        self.set_content(toolbar)

    def _build_sidebar(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        box.set_margin_top(12)
        box.set_margin_bottom(12)
        box.set_margin_start(12)
        box.set_margin_end(12)

        for label in [
            "System",
            "Appearance",
            "Display",
            "Sound",
            "Network",
            "Storage",
            "Performance",
            "Accessibility",
            "Build",
            "Diagnostics",
        ]:
            row = Gtk.Button(label=label)
            row.add_css_class("flat")
            row.set_halign(Gtk.Align.FILL)
            row.connect("clicked", self._show_page, label)
            box.append(row)

        return box

    def _show_page(self, _button, label):
        self.content_page.set_title(label)
        self.content_page.set_child(self._build_content(label))

    def _build_content(self, category):
        page = Adw.PreferencesPage()
        page.set_title(category)
        release = read_os_release()
        profile = read_profile_env()

        if category == "System":
            group = Adw.PreferencesGroup(title="ZenithOS")
            group.add(Adw.ActionRow(title="Edition", subtitle=release.get("PRETTY_NAME", "ZenithOS Workstation")))
            group.add(Adw.ActionRow(title="Kernel", subtitle=platform.release()))
            group.add(Adw.ActionRow(title="Memory", subtitle=memory_summary()))
            group.add(Adw.ActionRow(title="Root storage", subtitle=disk_summary("/")))
            group.add(Adw.ActionRow(title="CPU", subtitle=platform.processor() or first_line(command_output(["uname", "-m"]))))
            group.add(Adw.ActionRow(title="Session", subtitle=os.environ.get("XDG_SESSION_TYPE", "unknown")))
            group.add(Adw.ActionRow(title="Hardware profile", subtitle=f"{profile['ZENITH_PROFILE_NAME']} ({profile['ZENITH_PROFILE_ID']})"))
            group.add(Adw.ActionRow(title="Profile source", subtitle=profile["ZENITH_PROFILE_SOURCE"]))
            page.add(group)

            services = Adw.PreferencesGroup(title="Services")
            services.add(Adw.ActionRow(title="SDDM", subtitle=service_status("sddm.service")))
            services.add(Adw.ActionRow(title="NetworkManager", subtitle=service_status("NetworkManager.service")))
            services.add(Adw.ActionRow(title="PipeWire", subtitle=service_status("pipewire.service")))
            services.add(Adw.ActionRow(title="Polkit", subtitle=service_status("polkit.service")))
            page.add(services)

        elif category == "Appearance":
            group = Adw.PreferencesGroup(title="Desktop")
            group.add(Adw.ActionRow(title="Shell", subtitle="Zenith panel layout on KDE Plasma internals"))
            group.add(Adw.ActionRow(title="Wallpaper", subtitle="/usr/share/backgrounds/zenith/zenith-login-wallpaper.png"))
            group.add(Adw.ActionRow(title="Cursor", subtitle="Breeze, size 48 for VM visibility"))
            group.add(Adw.ActionRow(title="Theme", subtitle="Breeze Dark defaults with Zenith wallpaper and custom layout"))
            group.add(self._switch_row(
                "Animations",
                "Toggle compositor animations for responsiveness testing",
                gsettings_bool("org.gnome.desktop.interface", "enable-animations"),
                [
                    ["gsettings", "set", "org.gnome.desktop.interface", "enable-animations", "true"],
                    ["kwriteconfig6", "--file", "kwinrc", "--group", "Plugins", "--key", "contrastEnabled", "true"],
                    ["kwriteconfig6", "--file", "kwinrc", "--group", "Plugins", "--key", "blurEnabled", "true"]
                ],
                [
                    ["gsettings", "set", "org.gnome.desktop.interface", "enable-animations", "false"],
                    ["kwriteconfig6", "--file", "kwinrc", "--group", "Plugins", "--key", "contrastEnabled", "false"],
                    ["kwriteconfig6", "--file", "kwinrc", "--group", "Plugins", "--key", "blurEnabled", "false"]
                ],
            ))
            page.add(group)

            actions = Adw.PreferencesGroup(title="Actions")
            actions.add(self._button_row("Color scheme", "Switch between dark and light Breeze modes", [
                ("Dark", [
                    ["gsettings", "set", "org.gnome.desktop.interface", "color-scheme", "prefer-dark"],
                    ["kwriteconfig6", "--file", "kdeglobals", "--group", "General", "--key", "ColorScheme", "BreezeDark"],
                    ["kwriteconfig6", "--file", "kdeglobals", "--group", "Colors", "--key", "theme", "BreezeDark"]
                ]),
                ("Light", [
                    ["gsettings", "set", "org.gnome.desktop.interface", "color-scheme", "prefer-light"],
                    ["kwriteconfig6", "--file", "kdeglobals", "--group", "General", "--key", "ColorScheme", "BreezeLight"],
                    ["kwriteconfig6", "--file", "kdeglobals", "--group", "Colors", "--key", "theme", "BreezeLight"]
                ]),
            ]))
            actions.add(self._button_row("Cursor size", "Adjust pointer visibility", [
                ("32", [
                    ["gsettings", "set", "org.gnome.desktop.interface", "cursor-size", "32"],
                    ["kwriteconfig6", "--file", "kcminputrc", "--group", "Mouse", "--key", "cursorSize", "32"]
                ]),
                ("48", [
                    ["gsettings", "set", "org.gnome.desktop.interface", "cursor-size", "48"],
                    ["kwriteconfig6", "--file", "kcminputrc", "--group", "Mouse", "--key", "cursorSize", "48"]
                ]),
                ("64", [
                    ["gsettings", "set", "org.gnome.desktop.interface", "cursor-size", "64"],
                    ["kwriteconfig6", "--file", "kcminputrc", "--group", "Mouse", "--key", "cursorSize", "64"]
                ]),
            ]))
            actions.add(self._launch_row("Open KDE Settings", "Open the upstream system settings panel", ["systemsettings"]))
            actions.add(self._terminal_row("Reset Zenith session defaults", "Reapply background, favorites, and Plasma config", "/usr/lib/zenith/zenith-session-setup"))
            page.add(actions)

        elif category == "Display":
            group = Adw.PreferencesGroup(title="Display")
            group.add(Adw.ActionRow(title="Session type", subtitle=os.environ.get("XDG_SESSION_TYPE", "unknown")))
            group.add(Adw.ActionRow(title="Display server", subtitle="Wayland through KDE Plasma/KWin"))
            group.add(Adw.ActionRow(title="VM cursor", subtitle="USB tablet input plus Adwaita cursor size 48"))
            group.add(Adw.ActionRow(title="Graphics", subtitle=first_line(command_output(["sh", "-c", "lspci 2>/dev/null | grep -Ei 'vga|display|3d' | head -1"]))))
            page.add(group)

            actions = Adw.PreferencesGroup(title="Actions")
            actions.add(self._launch_row("Open display panel", "Resolution, scaling, rotation, and monitor layout", ["systemsettings", "kcm_kscreen"]))
            actions.add(self._launch_row("Open mouse and touchpad", "Pointer speed, touchpad, and click behavior", ["systemsettings", "kcm_touchpad"]))
            actions.add(self._terminal_row("Display diagnostics", "List active session, graphics devices, and compositor logs", "echo XDG_SESSION_TYPE=$XDG_SESSION_TYPE; echo; lspci 2>/dev/null | grep -Ei 'vga|display|3d' || true; exec bash"))
            page.add(actions)

        elif category == "Sound":
            group = Adw.PreferencesGroup(title="Audio")
            group.add(Adw.ActionRow(title="PipeWire", subtitle=service_status("pipewire.service")))
            group.add(Adw.ActionRow(title="WirePlumber", subtitle=service_status("wireplumber.service")))
            group.add(Adw.ActionRow(title="Pulse compatibility", subtitle=service_status("pipewire-pulse.service")))
            group.add(Adw.ActionRow(title="Audio control", subtitle=command_status("wpctl")))
            page.add(group)

            actions = Adw.PreferencesGroup(title="Actions")
            actions.add(self._button_row("Output volume", "Control the default PipeWire sink", [
                ("Down", ["wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "5%-"]),
                ("Mute", ["wpctl", "set-mute", "@DEFAULT_AUDIO_SINK@", "toggle"]),
                ("Up", ["wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "5%+"]),
            ]))
            actions.add(self._launch_row("Open sound panel", "Input, output, and volume controls", ["systemsettings", "kcm_pulseaudio"]))
            actions.add(self._terminal_row("List audio devices", "Run wpctl status in Terminal", "wpctl status; exec bash"))
            page.add(actions)

        elif category == "Performance":
            group = Adw.PreferencesGroup(title="Responsiveness")
            group.add(Adw.ActionRow(title="Animations", subtitle="Disabled for lower compositor latency"))
            group.add(Adw.ActionRow(title="Boot menu", subtitle="1 second default path; diagnostics stay available from GRUB"))
            group.add(Adw.ActionRow(title="Network wait", subtitle="wait-online units masked so Wi-Fi cannot block the desktop"))
            group.add(Adw.ActionRow(title="Live config", subtitle="Masked; Zenith preconfigures the live user and desktop at compose time"))
            group.add(Adw.ActionRow(title="Post-update cache rebuilds", subtitle="/etc/.updated and /var/.updated are stamped during compose"))
            group.add(Adw.ActionRow(title="Hardware profile", subtitle=f"{profile['ZENITH_PROFILE_NAME']} via {profile['ZENITH_PROFILE_SOURCE']}"))
            group.add(Adw.ActionRow(title="Power policy", subtitle=profile["ZENITH_PROFILE_POWER_POLICY"]))
            group.add(Adw.ActionRow(title="Graphics stack", subtitle=profile["ZENITH_PROFILE_GRAPHICS_STACK"]))
            group.add(Adw.ActionRow(title="Input stack", subtitle=profile["ZENITH_PROFILE_INPUT_STACK"]))
            group.add(Adw.ActionRow(title="Boot reports", subtitle="Only enabled from the diagnostics boot entry"))
            group.add(Adw.ActionRow(title="Entropy", subtitle="virtio-rng enabled in the run target"))
            group.add(Adw.ActionRow(title="Background timers", subtitle="Daily apt/man-db/e2scrub timers disabled in the live image"))
            page.add(group)

            actions = Adw.PreferencesGroup(title="Actions")
            actions.add(self._button_row("Power mode", "Apply a power-profiles-daemon mode now", [
                ("Saver", ["powerprofilesctl", "set", "power-saver"]),
                ("Balanced", ["powerprofilesctl", "set", "balanced"]),
                ("Performance", ["powerprofilesctl", "set", "performance"]),
            ]))
            actions.add(self._terminal_row("Set hardware override", "Persist a profile choice for the next boot", "printf 'Choose one: dev-vm, lenovo-v14-ada, generic-workstation\\n'; read -r profile; case \"$profile\" in dev-vm|lenovo-v14-ada|generic-workstation) sudo mkdir -p /etc/zenith; echo \"$profile\" | sudo tee /etc/zenith/hardware-profile; sudo /usr/lib/zenith/zenith-hardware-detect; cat /run/zenith/hardware-profile.env;; *) echo 'invalid profile';; esac; exec bash"))
            actions.add(self._terminal_row("Re-detect hardware", "Run Zenith hardware detection again", "sudo /usr/lib/zenith/zenith-hardware-detect; cat /run/zenith/hardware-profile.env; exec bash"))
            actions.add(self._terminal_row("Boot blame", "Show slowest systemd units", "systemd-analyze blame; echo; systemd-analyze critical-chain; exec bash"))
            actions.add(self._terminal_row("Service failures", "List failed units and recent errors", "systemctl --failed; echo; journalctl -b -p warning --no-pager | tail -160; exec bash"))
            page.add(actions)

        elif category == "Network":
            group = Adw.PreferencesGroup(title="Network")
            group.add(Adw.ActionRow(title="NetworkManager", subtitle=service_status("NetworkManager.service")))
            group.add(Adw.ActionRow(title="iwd", subtitle=command_status("iwd")))
            group.add(Adw.ActionRow(title="Addresses", subtitle=first_line(command_output(["sh", "-c", "ip -brief address 2>/dev/null | grep -v LOOPBACK | head -1"]))))
            group.add(Adw.ActionRow(title="Default route", subtitle=first_line(command_output(["sh", "-c", "ip route show default 2>/dev/null | head -1"]))))
            group.add(Adw.ActionRow(title="Early boot", subtitle="/etc/network/interfaces contains loopback for live-boot compatibility"))
            page.add(group)

            actions = Adw.PreferencesGroup(title="Actions")
            actions.add(self._button_row("Wi-Fi radio", "Control NetworkManager Wi-Fi state", [
                ("On", ["nmcli", "radio", "wifi", "on"]),
                ("Off", ["nmcli", "radio", "wifi", "off"]),
                ("Rescan", ["nmcli", "device", "wifi", "rescan"]),
            ]))
            actions.add(self._launch_row("Open network panel", "Wi-Fi, wired, VPN, and proxy settings", ["systemsettings", "kcm_networkmanagement"]))
            actions.add(self._terminal_row("Wi-Fi networks", "List visible Wi-Fi networks", "nmcli device wifi list; exec bash"))
            actions.add(self._terminal_row("Network diagnostics", "Show devices, addresses, and routes", "nmcli device status; echo; ip -brief address; echo; ip route; exec bash"))
            page.add(actions)

        elif category == "Storage":
            group = Adw.PreferencesGroup(title="30GB Budget")
            group.add(Adw.ActionRow(title="Root filesystem", subtitle=disk_summary("/")))
            group.add(Adw.ActionRow(title="APT cache cap", subtitle="1536 MB target"))
            group.add(Adw.ActionRow(title="Flatpak reserve", subtitle="4 GB target"))
            group.add(Adw.ActionRow(title="Self-host builds", subtitle="6 GB target for generated rootfs and ISO artifacts"))
            group.add(Adw.ActionRow(title="Logs", subtitle="512 MB target"))
            page.add(group)

            actions = Adw.PreferencesGroup(title="Actions")
            actions.add(self._terminal_row("Storage audit", "Measure package, Flatpak, logs, and build outputs", "df -h /; echo; du -sh /var/cache/apt/archives /var/lib/apt/lists /var/lib/flatpak /var/log /var/lib/zenith-build 2>/dev/null; exec bash"))
            actions.add(self._terminal_row("Clean caches", "Run apt clean and journal vacuum", "sudo apt clean; sudo journalctl --vacuum-size=512M; exec bash"))
            page.add(actions)

        elif category == "Accessibility":
            group = Adw.PreferencesGroup(title="Accessibility")
            group.add(Adw.ActionRow(title="AT-SPI", subtitle=service_status("at-spi-dbus-bus.service")))
            group.add(Adw.ActionRow(title="Orca", subtitle=command_status("orca")))
            group.add(Adw.ActionRow(title="High visibility cursor", subtitle="Enabled through cursor-size=48"))
            group.add(Adw.ActionRow(title="Animations", subtitle="Disabled by default for lower motion and faster UI"))
            page.add(group)

            actions = Adw.PreferencesGroup(title="Actions")
            actions.add(self._button_row("Screen reader", "Start or stop Orca for accessibility testing", [
                ("Start", ["orca"]),
                ("Stop", ["pkill", "-f", "orca"]),
            ]))
            actions.add(self._launch_row("Open accessibility panel", "KDE accessibility controls", ["systemsettings", "kcm_accessibility"]))
            page.add(actions)

        elif category == "Build":
            group = Adw.PreferencesGroup(title="Build Environment")
            group.add(Adw.ActionRow(title="Package manager", subtitle=f"apt: {command_status('apt')}; flatpak: {command_status('flatpak')}"))
            group.add(Adw.ActionRow(title="Image builder", subtitle=f"mmdebstrap: {command_status('mmdebstrap')}; xorriso: {command_status('xorriso')}"))
            group.add(Adw.ActionRow(title="Source tree", subtitle=f"/usr/src/zenithos ({file_status('/usr/src/zenithos/Makefile')})"))
            group.add(Adw.ActionRow(title="Build command", subtitle="zenith-build rootfs; zenith-build iso"))
            page.add(group)

            actions = Adw.PreferencesGroup(title="Actions")
            actions.add(self._terminal_row("Run self-host check", "Validate manifests and workstation structure", "cd /usr/src/zenithos && zenith-build check; exec bash"))
            actions.add(self._terminal_row("Build next rootfs", "Start the self-hosted rootfs build flow", "cd /usr/src/zenithos && zenith-build rootfs; exec bash"))
            actions.add(self._terminal_row("Build next ISO", "Compose an ISO from the latest self-hosted rootfs", "cd /usr/src/zenithos && zenith-build iso; exec bash"))
            page.add(actions)

        elif category == "Diagnostics":
            group = Adw.PreferencesGroup(title="Diagnostics")
            group.add(Adw.ActionRow(title="Hardware detector", subtitle=service_status("zenith-hardware-detect.service")))
            group.add(Adw.ActionRow(title="Profile decision", subtitle=profile["ZENITH_PROFILE_REASON"]))
            group.add(Adw.ActionRow(title="Boot report service", subtitle=service_status("zenith-boot-report.service")))
            group.add(Adw.ActionRow(title="First boot marker", subtitle=file_status("/run/zenithos-graphical-boot")))
            group.add(Adw.ActionRow(title="Kernel command line", subtitle=first_line(command_output(["cat", "/proc/cmdline"]))))
            page.add(group)

            actions = Adw.PreferencesGroup(title="Actions")
            actions.add(self._terminal_row("Open boot journal", "Show current boot logs", "journalctl -b --no-pager | tail -260; exec bash"))
            actions.add(self._terminal_row("Open display logs", "Show SDDM and Zenith handoff logs", "journalctl -b -u sddm.service -u display-manager.service -u zenith-plymouth-handoff.service --no-pager; exec bash"))
            actions.add(self._terminal_row("Hardware summary", "Show profile, CPU, memory, graphics, block devices, and USB devices", "cat /run/zenith/hardware-profile.env 2>/dev/null; echo; uname -a; echo; lscpu | head -30; echo; free -h; echo; lspci 2>/dev/null | grep -Ei 'vga|display|network|audio' || true; echo; lsblk; exec bash"))
            actions.add(self._terminal_row("Persistence status", "Check whether live-boot persistence is active", "findmnt -no SOURCE,TARGET /; echo; grep -R . /run/live/persistence 2>/dev/null || true; echo; lsblk -o NAME,LABEL,SIZE,FSTYPE,MOUNTPOINTS; exec bash"))
            page.add(actions)

        return page

    def _launch_row(self, title, subtitle, command):
        row = Adw.ActionRow(title=title, subtitle=subtitle)
        button = Gtk.Button(label="Open")
        button.connect("clicked", self._launch, command)
        row.add_suffix(button)
        row.set_activatable_widget(button)
        return row

    def _button_row(self, title, subtitle, actions):
        row = Adw.ActionRow(title=title, subtitle=subtitle)
        for label, command in actions:
            button = Gtk.Button(label=label)
            button.connect("clicked", self._launch, command)
            row.add_suffix(button)
        return row

    def _switch_row(self, title, subtitle, active, command_on, command_off):
        row = Adw.ActionRow(title=title, subtitle=subtitle)
        control = Gtk.Switch(active=active)
        control.set_valign(Gtk.Align.CENTER)
        control.connect("state-set", self._switch_changed, command_on, command_off)
        row.add_suffix(control)
        row.set_activatable_widget(control)
        return row

    def _switch_changed(self, _switch, state, command_on, command_off):
        command = command_on if state else command_off
        try:
            if command and isinstance(command[0], list):
                for cmd in command:
                    subprocess.Popen(cmd)
            else:
                subprocess.Popen(command)
        except (OSError, IndexError):
            pass
        return False

    def _terminal_row(self, title, subtitle, command):
        return self._launch_row(title, subtitle, ["zenith-terminal", "--command", command])

    def _launch(self, _button, command):
        try:
            if command and isinstance(command[0], list):
                for cmd in command:
                    subprocess.Popen(cmd)
            else:
                subprocess.Popen(command)
        except (OSError, IndexError):
            pass


class SettingsApp(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id="os.zenith.Settings",
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )

    def do_activate(self):
        window = self.props.active_window
        if window is None:
            window = SettingsWindow(self)
        window.present()


def main():
    app = SettingsApp()
    return app.run(None)


if __name__ == "__main__":
    raise SystemExit(main())
