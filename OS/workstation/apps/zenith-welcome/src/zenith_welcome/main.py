import os
import shutil
import subprocess

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gio, Gtk


def run_status(command, timeout=3):
    try:
        result = subprocess.run(
            command,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired):
        return "Unavailable"
    text = result.stdout.strip() or result.stderr.strip()
    return text if text else "OK"


def command_available(command):
    return "Ready" if shutil.which(command) else "Missing"


def file_status(path):
    return "Present" if os.path.exists(path) else "Missing"


def readable_size(path):
    try:
        size = os.path.getsize(path)
    except OSError:
        return "Unavailable"
    for unit in ["B", "KB", "MB", "GB"]:
        if size < 1024 or unit == "GB":
            return f"{size:.0f} {unit}" if unit == "B" else f"{size:.1f} {unit}"
        size /= 1024
    return "Unavailable"


def os_name():
    try:
        with open("/etc/os-release", "r", encoding="utf-8") as handle:
            for line in handle:
                if line.startswith("PRETTY_NAME="):
                    return line.split("=", 1)[1].strip().strip('"')
    except OSError:
        pass
    return "ZenithOS Workstation"


class WelcomeWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="Welcome to ZenithOS")
        self.set_default_size(920, 640)

        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title="Welcome", subtitle=os_name()))
        toolbar.add_top_bar(header)
        toolbar.set_content(self._build_content())
        self.set_content(toolbar)

    def _build_content(self):
        scroller = Gtk.ScrolledWindow()
        page = Adw.PreferencesPage()
        page.set_title("Welcome")
        scroller.set_child(page)

        start = Adw.PreferencesGroup(title="Start Here")
        for title, subtitle, command in [
            ("Open Settings", "Check system, desktop, service, and build status", "zenith-settings"),
            ("Open Terminal", "Run shell commands and self-host build tools", "zenith-terminal"),
            ("Open Files", "Browse the live user workspace", "zenith-files"),
            ("Open Packages", "Inspect apt, Flatpak, and Zenith package state", "zenith-packages"),
            ("Open Installer", "Preview install readiness without changing disks", "zenith-installer"),
        ]:
            start.add(self._action_row(title, subtitle, command))
        page.add(start)

        status = Adw.PreferencesGroup(title="System Status")
        status.add(Adw.ActionRow(title="Session", subtitle=os.environ.get("XDG_SESSION_TYPE", "unknown")))
        status.add(Adw.ActionRow(title="Kernel", subtitle=run_status(["uname", "-r"])))
        status.add(Adw.ActionRow(title="Network", subtitle=run_status(["systemctl", "is-active", "NetworkManager.service"])))
        status.add(Adw.ActionRow(title="APT", subtitle=command_available("apt")))
        status.add(Adw.ActionRow(title="Flatpak", subtitle=command_available("flatpak")))
        status.add(Adw.ActionRow(title="Self-host Builder", subtitle=command_available("zenith-build")))
        page.add(status)

        desktop = Adw.PreferencesGroup(title="Desktop Readiness")
        desktop.add(Adw.ActionRow(title="ZenithShell extension", subtitle=file_status("/usr/share/gnome-shell/extensions/zenith-shell@zenithos.local/extension.js")))
        desktop.add(Adw.ActionRow(title="Zenith wallpaper", subtitle=file_status("/usr/share/backgrounds/zenith/zenith-login-wallpaper.png")))
        desktop.add(Adw.ActionRow(title="GDM autologin", subtitle=file_status("/etc/gdm3/custom.conf")))
        desktop.add(Adw.ActionRow(title="Visible cursor default", subtitle="Adwaita cursor size 48"))
        page.add(desktop)

        boot = Adw.PreferencesGroup(title="Boot Health")
        boot.add(Adw.ActionRow(title="Live config", subtitle="Disabled at compose time for faster boot"))
        boot.add(Adw.ActionRow(title="Post-update cache rebuilds", subtitle="Stamped at compose time"))
        boot.add(Adw.ActionRow(title="Seed ISO", subtitle=readable_size("/usr/src/zenithos/build/workstation/zenithos-seed.iso")))
        boot.add(self._terminal_row("Boot log", "Open the current boot journal in Zenith Terminal", "journalctl -b --no-pager | tail -260; exec bash"))
        boot.add(self._terminal_row("Boot blame", "See slowest systemd units and critical chain", "systemd-analyze blame; echo; systemd-analyze critical-chain; exec bash"))
        page.add(boot)

        build = Adw.PreferencesGroup(title="Build Path")
        build.add(Adw.ActionRow(title="Seed ISO", subtitle="Boots a live GNOME-internals ZenithOS session"))
        build.add(Adw.ActionRow(title="Self-hosting", subtitle="zenith-build will compose rootfs and ISO from inside ZenithOS"))
        build.add(Adw.ActionRow(title="Install Mode", subtitle="Preview now; disk writes stay locked until installer policy is finalized"))
        build.add(self._terminal_row("Run build check", "Validate the source tree from inside ZenithOS", "cd /usr/src/zenithos && zenith-build check; exec bash"))
        page.add(build)

        return scroller

    def _action_row(self, title, subtitle, command):
        row = Adw.ActionRow(title=title, subtitle=subtitle)
        button = Gtk.Button(label="Open")
        button.add_css_class("suggested-action")
        button.connect("clicked", self._launch, command)
        row.add_suffix(button)
        row.set_activatable_widget(button)
        return row

    def _terminal_row(self, title, subtitle, command):
        row = Adw.ActionRow(title=title, subtitle=subtitle)
        button = Gtk.Button(label="Open")
        button.connect("clicked", self._launch_terminal, command)
        row.add_suffix(button)
        row.set_activatable_widget(button)
        return row

    def _launch(self, _button, command):
        try:
            subprocess.Popen([command])
        except OSError as exc:
            dialog = Adw.MessageDialog(
                transient_for=self,
                heading="Unable to launch",
                body=f"{command}: {exc}",
            )
            dialog.add_response("close", "Close")
            dialog.present()

    def _launch_terminal(self, _button, command):
        try:
            subprocess.Popen(["zenith-terminal", "--command", command])
        except OSError as exc:
            dialog = Adw.MessageDialog(
                transient_for=self,
                heading="Unable to launch terminal",
                body=str(exc),
            )
            dialog.add_response("close", "Close")
            dialog.present()


class WelcomeApp(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id="os.zenith.Welcome",
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )

    def do_activate(self):
        window = self.props.active_window
        if window is None:
            window = WelcomeWindow(self)
        window.present()


def main():
    app = WelcomeApp()
    return app.run(None)


if __name__ == "__main__":
    raise SystemExit(main())
