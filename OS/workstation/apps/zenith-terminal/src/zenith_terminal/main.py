import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Gdk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gdk, Gio, GLib, Gtk

try:
    gi.require_version("Vte", "3.91")
    from gi.repository import Vte
except (ImportError, ValueError):
    Vte = None


DEFAULT_PROFILES = {
    "zenith-dark": {
        "foreground": "#f4f7f8",
        "background": "#071017",
        "accent": "#4aa78a",
        "scrollback": 10000,
    },
    "zenith-light": {
        "foreground": "#1c2228",
        "background": "#f7f8f6",
        "accent": "#277a61",
        "scrollback": 10000,
    },
    "solarized": {
        "foreground": "#839496",
        "background": "#002b36",
        "accent": "#268bd2",
        "scrollback": 10000,
    },
}


class ProfileStore:
    def __init__(self, path=None):
        self.path = Path(path or "~/.config/zenith-terminal/profiles.json").expanduser()
        self.profiles = dict(DEFAULT_PROFILES)
        self.active = "zenith-dark"
        self.load()

    def load(self):
        if not self.path.exists():
            return
        try:
            payload = json.loads(self.path.read_text(encoding="utf-8"))
            profiles = payload.get("profiles", {})
            active = payload.get("active", self.active)
            if not isinstance(profiles, dict) or active not in profiles | DEFAULT_PROFILES:
                raise ValueError("invalid profile payload")
            for name, profile in profiles.items():
                if self._valid_profile(profile):
                    self.profiles[name] = profile
            if active in self.profiles:
                self.active = active
        except (OSError, json.JSONDecodeError, ValueError, TypeError):
            self.profiles = dict(DEFAULT_PROFILES)
            self.active = "zenith-dark"

    def _valid_profile(self, profile):
        return (
            isinstance(profile, dict)
            and isinstance(profile.get("foreground"), str)
            and isinstance(profile.get("background"), str)
        )

    def current(self):
        profile = dict(self.profiles.get(self.active, DEFAULT_PROFILES["zenith-dark"]))
        scrollback = int(profile.get("scrollback", 10000))
        profile["scrollback"] = max(1000, min(scrollback, 10000))
        return profile


class ANSIStateMachine:
    TEXT = 0
    ESC = 1
    CSI = 2
    OSC = 3

    def __init__(self):
        self.state = self.TEXT
        self.params = ""
        self.cursor = [0, 0]
        self.color = 0
        self.clears = 0

    @property
    def state_count(self):
        return 4

    def feed(self, text):
        for char in text:
            if self.state == self.TEXT:
                if char == "\x1b":
                    self.state = self.ESC
            elif self.state == self.ESC:
                if char == "[":
                    self.params = ""
                    self.state = self.CSI
                elif char == "]":
                    self.state = self.OSC
                else:
                    self.state = self.TEXT
            elif self.state == self.CSI:
                if char.isdigit() or char in ";?":
                    self.params += char
                else:
                    self._apply_csi(char)
                    self.state = self.TEXT
            elif self.state == self.OSC:
                if char == "\a":
                    self.state = self.TEXT

    def _numbers(self):
        values = []
        for chunk in self.params.replace("?", "").split(";"):
            if not chunk:
                continue
            try:
                values.append(int(chunk))
            except ValueError:
                values.append(0)
        return values

    def _apply_csi(self, command):
        values = self._numbers()
        amount = values[0] if values else 1
        if command == "A":
            self.cursor[1] = max(0, self.cursor[1] - amount)
        elif command == "B":
            self.cursor[1] += amount
        elif command == "C":
            self.cursor[0] += amount
        elif command == "D":
            self.cursor[0] = max(0, self.cursor[0] - amount)
        elif command in "Hf":
            self.cursor = [values[1] - 1 if len(values) > 1 else 0, values[0] - 1 if values else 0]
        elif command == "J":
            self.clears += 1
        elif command == "m":
            self.color = values[-1] if values else 0


def make_rgba(value):
    color = Gdk.RGBA()
    color.parse(value)
    return color


class TerminalWindow(Adw.ApplicationWindow):
    def __init__(self, app, command=None):
        super().__init__(application=app, title="Zenith Terminal")
        self.command = command
        self.profile_store = ProfileStore()
        self.profile = self.profile_store.current()
        self.parser = ANSIStateMachine()
        self.panes = []
        self.broadcast = False
        self.set_default_size(1040, 680)

        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        subtitle = "ZenithOS" if command is None else "Command session"
        header.set_title_widget(Adw.WindowTitle(title="Terminal", subtitle=subtitle))
        self._build_header(header)
        toolbar.add_top_bar(header)
        toolbar.set_content(self._build_content())
        self.set_content(toolbar)

    def _build_header(self, header):
        actions = [
            ("Boot Log", "journalctl -b --no-pager | tail -240; exec bash"),
            ("Updates", "apt list --upgradable; echo; flatpak remote-ls --updates 2>/dev/null || true; exec bash"),
            ("Build", "cd /usr/src/zenithos 2>/dev/null || cd ~; zenith-build check; exec bash"),
        ]
        for label, command_line in actions:
            button = Gtk.Button(label=label)
            button.add_css_class("flat")
            button.connect("clicked", self._open_command, command_line)
            header.pack_end(button)

        for label, callback in [
            ("Tab", self._new_tab),
            ("Split H", self._split_horizontal),
            ("Split V", self._split_vertical),
            ("Broadcast", self._toggle_broadcast),
        ]:
            button = Gtk.Button(label=label)
            button.connect("clicked", callback)
            header.pack_start(button)

    def _build_content(self):
        if Vte is not None:
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
            self.tab_view = Adw.TabView()
            tab_bar = Adw.TabBar()
            tab_bar.set_view(self.tab_view)
            box.append(tab_bar)
            box.append(self.tab_view)
            self._add_tab(self.command, title="Shell")
            return box

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
        box.set_margin_top(28)
        box.set_margin_bottom(28)
        box.set_margin_start(28)
        box.set_margin_end(28)

        title = Gtk.Label(label="Terminal runtime pending")
        title.add_css_class("title-2")
        title.set_halign(Gtk.Align.START)
        box.append(title)

        body = Gtk.Label(label="The Zenith Terminal shell is installed. VTE will be enabled when the runtime package is present.")
        body.set_wrap(True)
        body.set_halign(Gtk.Align.START)
        box.append(body)

        preview = Gtk.TextView()
        preview.set_editable(False)
        preview.set_monospace(True)
        text = "zenith@workstation:~$ _"
        if self.command:
            text = f"VTE unavailable. Requested command:\n\n{self.command}\n"
        preview.buffer.set_text(text)
        box.append(preview)
        return box

    def _shell_argv(self, command):
        if command:
            return ["/bin/bash", "-lc", f"{command}; status=$?; echo; echo \"[exit $status]\"; exec /bin/bash"]
        return [os.environ.get("SHELL", "/bin/bash")]

    def _create_terminal(self, command=None):
        terminal = Vte.Terminal()
        terminal.set_colors(make_rgba(self.profile["foreground"]), make_rgba(self.profile["background"]), [])
        terminal.set_scrollback_lines(self.profile["scrollback"])
        self._install_url_matcher(terminal)

        controller = Gtk.EventControllerKey()
        controller.connect("key-pressed", self._broadcast_key, terminal)
        terminal.add_controller(controller)

        argv = self._shell_argv(command)
        terminal.spawn_async(
            Vte.PtyFlags.DEFAULT,
            None,
            argv,
            None,
            GLib.SpawnFlags.DEFAULT,
            None,
            None,
            -1,
            None,
            None,
            None,
        )
        self.panes.append(terminal)
        return terminal

    def _install_url_matcher(self, terminal):
        if not hasattr(Vte, "Regex"):
            return
        try:
            regex = Vte.Regex.new_for_match(r"https?://[^\s<>()]+", -1, 0)
            terminal.match_add_regex(regex, 0)
            terminal.set_cursor_blink_mode(Vte.CursorBlinkMode.ON)
        except Exception:
            return

    def _active_container(self):
        page = self.tab_view.get_selected_page()
        return page.get_child() if page is not None else None

    def _add_tab(self, command=None, title="Shell"):
        container = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        container.append(self._create_terminal(command))
        page = self.tab_view.append(container)
        page.set_title(title)
        self.tab_view.set_selected_page(page)
        return page

    def _new_tab(self, _button):
        self._add_tab(None, title="Shell")

    def _split(self, orientation):
        container = self._active_container()
        if container is None:
            return
        child = container.get_first_child()
        if child is None:
            return
        container.remove(child)
        pane = Gtk.Paned(orientation=orientation)
        pane.set_start_child(child)
        pane.set_end_child(self._create_terminal(None))
        pane.set_resize_start_child(True)
        pane.set_resize_end_child(True)
        container.append(pane)

    def _split_horizontal(self, _button):
        self._split(Gtk.Orientation.VERTICAL)

    def _split_vertical(self, _button):
        self._split(Gtk.Orientation.HORIZONTAL)

    def _toggle_broadcast(self, button):
        self.broadcast = not self.broadcast
        button.set_label("Broadcast On" if self.broadcast else "Broadcast")

    def _broadcast_key(self, _controller, keyval, _keycode, _state, source):
        if not self.broadcast or Vte is None:
            return False
        text = chr(keyval) if 0 <= keyval < 128 else ""
        if not text:
            return False
        payload = text.encode("utf-8")
        for terminal in self.panes:
            if terminal is not source:
                try:
                    terminal.feed_child(payload)
                except Exception:
                    pass
        return False

    def _open_command(self, _button, command):
        try:
            subprocess.Popen(["zenith-terminal", "--command", command])
        except OSError:
            pass


class TerminalApp(Adw.Application):
    def __init__(self, command=None):
        super().__init__(
            application_id="os.zenith.Terminal",
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )
        self.command = command

    def do_activate(self):
        window = self.props.active_window
        if window is None:
            window = TerminalWindow(self, command=self.command)
        window.present()


def parse_args(argv):
    parser = argparse.ArgumentParser(prog="zenith-terminal")
    parser.add_argument("--command", "-e", help="run a command in a Zenith Terminal session")
    parser.add_argument("--ansi-self-test", action="store_true", help="run the ANSI parser stress test")
    return parser.parse_args(argv)


def ansi_self_test():
    parser = ANSIStateMachine()
    sample = "\x1b[31mred\x1b[0m\x1b[2J\x1b[10;20H"
    for _ in range(1000):
        parser.feed(sample)
    print(f"ansi-states={parser.state_count} clears={parser.clears} cursor={parser.cursor[0]},{parser.cursor[1]}")
    return 0


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    if args.ansi_self_test:
        return ansi_self_test()
    app = TerminalApp(command=args.command)
    return app.run(None)


if __name__ == "__main__":
    raise SystemExit(main())
