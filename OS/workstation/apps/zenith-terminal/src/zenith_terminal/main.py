import argparse
import os
import subprocess
import sys

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


class TerminalWindow(Adw.ApplicationWindow):
    def __init__(self, app, command=None):
        super().__init__(application=app, title="Zenith Terminal")
        self.command = command
        self.set_default_size(980, 620)

        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        subtitle = "ZenithOS" if command is None else "Command session"
        title = Adw.WindowTitle(title="Terminal", subtitle=subtitle)
        header.set_title_widget(title)
        for label, command_line in [
            ("Boot Log", "journalctl -b --no-pager | tail -240; exec bash"),
            ("Updates", "apt list --upgradable; echo; flatpak remote-ls --updates 2>/dev/null || true; exec bash"),
            ("Build", "cd /usr/src/zenithos 2>/dev/null || cd ~; zenith-build check; exec bash"),
        ]:
            button = Gtk.Button(label=label)
            button.add_css_class("flat")
            button.connect("clicked", self._open_command, command_line)
            header.pack_end(button)
        toolbar.add_top_bar(header)
        toolbar.set_content(self._build_content())
        self.set_content(toolbar)

    def _build_content(self):
        if Vte is not None:
            terminal = Vte.Terminal()
            foreground = Gdk.RGBA()
            background = Gdk.RGBA()
            foreground.parse("#f4f7f8")
            background.parse("#071017")
            terminal.set_colors(foreground, background, [])
            terminal.set_scrollback_lines(10000)
            argv = [os.environ.get("SHELL", "/bin/bash")]
            if self.command:
                argv = ["/bin/bash", "-lc", f"{self.command}; status=$?; echo; echo \"[exit $status]\"; exec /bin/bash"]
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
            return terminal

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
        box.set_margin_top(28)
        box.set_margin_bottom(28)
        box.set_margin_start(28)
        box.set_margin_end(28)

        title = Gtk.Label(label="Terminal runtime pending")
        title.add_css_class("title-2")
        title.set_halign(Gtk.Align.START)
        box.append(title)

        body = Gtk.Label(
            label="The Zenith Terminal shell is installed. VTE will be enabled when the runtime package is present."
        )
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
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    app = TerminalApp(command=args.command)
    return app.run(None)


if __name__ == "__main__":
    raise SystemExit(main())
