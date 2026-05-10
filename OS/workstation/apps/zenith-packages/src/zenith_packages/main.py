import os
import shutil
import subprocess

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gio, Gtk


def command_output(command):
    try:
        result = subprocess.run(
            command,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=2,
        )
    except (OSError, subprocess.TimeoutExpired):
        return "Unavailable"
    output = result.stdout.strip()
    return output if output else "Unavailable"


def available(command):
    return "Available" if shutil.which(command) else "Missing"


def local_repo_count():
    packages_file = "/var/lib/zenith-repo/Packages"
    try:
        with open(packages_file, "r", encoding="utf-8") as handle:
            return str(sum(1 for line in handle if line.startswith("Package: ")))
    except OSError:
        return "Unavailable"


def directory_size(path):
    total = 0
    for root, _dirs, files in os.walk(path):
        for name in files:
            try:
                total += os.path.getsize(os.path.join(root, name))
            except OSError:
                pass
    if total == 0:
        return "0 MB"
    return f"{total / 1024 / 1024:.0f} MB"


def terminal_command(command):
    return ["zenith-terminal", "--command", command]


class PackagesWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="Zenith Packages")
        self.set_default_size(1040, 700)

        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title="Packages", subtitle="apt + Flatpak"))
        toolbar.add_top_bar(header)
        toolbar.set_content(self._build_content())
        self.set_content(toolbar)

    def _build_content(self):
        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        outer.set_margin_top(12)
        outer.set_margin_bottom(12)
        outer.set_margin_start(12)
        outer.set_margin_end(12)

        page = Adw.PreferencesPage()
        page.set_title("Packages")
        outer.append(page)

        status = Adw.PreferencesGroup(title="Status")
        status.add(Adw.ActionRow(title="APT", subtitle=command_output(["apt", "--version"]).splitlines()[0]))
        status.add(Adw.ActionRow(title="Flatpak", subtitle=command_output(["flatpak", "--version"])))
        status.add(Adw.ActionRow(title="Zenith local packages", subtitle=f"{local_repo_count()} packages in /var/lib/zenith-repo"))
        page.add(status)

        storage = Adw.PreferencesGroup(title="Storage Budget")
        storage.add(Adw.ActionRow(title="APT cache", subtitle=directory_size("/var/cache/apt/archives")))
        storage.add(Adw.ActionRow(title="APT lists", subtitle=directory_size("/var/lib/apt/lists")))
        storage.add(Adw.ActionRow(title="Flatpak storage", subtitle=directory_size("/var/lib/flatpak")))
        storage.add(Adw.ActionRow(title="Build outputs", subtitle=directory_size("/var/lib/zenith-build")))
        storage.add(Adw.ActionRow(title="Flatpak support", subtitle=available("flatpak")))
        storage.add(Adw.ActionRow(title="Self-host build cap", subtitle="6 GB target for generated rootfs and ISO artifacts"))
        page.add(storage)

        search = Adw.PreferencesGroup(title="Package Search")
        search_row = Adw.ActionRow(title="APT search", subtitle="Search package names and descriptions")
        self.search_entry = Gtk.SearchEntry()
        self.search_entry.set_placeholder_text("package name")
        self.search_entry.set_width_chars(24)
        self.search_entry.connect("activate", self._run_search)
        search_button = Gtk.Button(label="Search")
        search_button.connect("clicked", self._run_search)
        search_row.add_suffix(self.search_entry)
        search_row.add_suffix(search_button)
        search.add(search_row)
        page.add(search)

        actions = Adw.PreferencesGroup(title="Inspect")
        for label, subtitle, command in [
            ("List system updates", "Read-only apt list --upgradable", ["apt", "list", "--upgradable"]),
            ("Show Flatpak remotes", "Read-only flatpak remotes", ["flatpak", "remotes"]),
            ("Storage audit", "Read-only package and build storage check", ["sh", "-c", "du -sh /var/cache/apt/archives /var/lib/apt/lists /var/lib/flatpak /var/lib/zenith-build 2>/dev/null"]),
            ("List Zenith repo", "Read-only local package repository list", ["sh", "-c", "grep '^Package: ' /var/lib/zenith-repo/Packages | sed 's/Package: //'"]),
        ]:
            row = Adw.ActionRow(title=label, subtitle=subtitle)
            button = Gtk.Button(label="Run")
            button.add_css_class("suggested-action")
            button.connect("clicked", self._run_action, label, command)
            row.add_suffix(button)
            row.set_activatable_widget(button)
            actions.add(row)
        page.add(actions)

        apply = Adw.PreferencesGroup(title="Update and Cleanup")
        for label, subtitle, command in [
            ("Refresh package metadata", "Runs apt update with sudo, then refreshes Flatpak metadata", "sudo apt update; flatpak update --appstream 2>/dev/null || true"),
            ("Apply APT upgrades", "Runs the normal apt upgrade flow in Terminal", "sudo apt update && sudo apt full-upgrade"),
            ("Update Flatpaks", "Runs flatpak update in Terminal", "flatpak update"),
            ("Clean package caches", "Runs apt clean and trims journal size to the Zenith budget", "sudo apt clean; sudo journalctl --vacuum-size=512M"),
            ("Remove unused Flatpaks", "Prunes unused Flatpak runtimes", "flatpak uninstall --unused"),
        ]:
            row = Adw.ActionRow(title=label, subtitle=subtitle)
            button = Gtk.Button(label="Open")
            button.connect("clicked", self._open_terminal_action, command)
            row.add_suffix(button)
            row.set_activatable_widget(button)
            apply.add(row)
        page.add(apply)

        self.output = Gtk.TextView()
        self.output.set_editable(False)
        self.output.set_monospace(True)
        self.output.set_vexpand(True)
        self.output.buffer.set_text("Select a read-only package action to inspect the live system.\n")

        output_frame = Gtk.Frame()
        output_frame.set_child(self.output)
        output_frame.set_vexpand(True)
        outer.append(output_frame)

        return outer

    def _run_search(self, _widget):
        query = self.search_entry.get_text().strip()
        if not query:
            self.output.buffer.set_text("Enter a package name or keyword first.\n")
            return
        self._run_command("APT search", ["apt-cache", "search", query])

    def _run_action(self, _button, label, command):
        self._run_command(label, command)

    def _run_command(self, label, command):
        self.output.buffer.set_text(f"$ {' '.join(command)}\n")
        try:
            result = subprocess.run(
                command,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                timeout=12,
            )
        except (OSError, subprocess.TimeoutExpired) as exc:
            self.output.buffer.set_text(f"{label}: {exc}\n")
            return

        output = result.stdout.strip()
        if not output:
            output = "No output."
        self.output.buffer.set_text(f"$ {' '.join(command)}\n\n{output}\n")

    def _open_terminal_action(self, _button, command):
        try:
            subprocess.Popen(terminal_command(command))
        except OSError as exc:
            self.output.buffer.set_text(f"Unable to open terminal action: {exc}\n")


class PackagesApp(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id="os.zenith.Packages",
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )

    def do_activate(self):
        window = self.props.active_window
        if window is None:
            window = PackagesWindow(self)
        window.present()


def main():
    app = PackagesApp()
    return app.run(None)


if __name__ == "__main__":
    raise SystemExit(main())
