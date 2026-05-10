import os
import subprocess
from datetime import datetime

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gio, Gtk


class FilesWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="Zenith Files")
        self.set_default_size(1040, 700)
        self.current_path = os.path.expanduser("~")
        self.show_hidden = False

        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        self.title_widget = Adw.WindowTitle(title="Files", subtitle=self.current_path)
        header.set_title_widget(self.title_widget)
        for label, callback in [
            ("Up", self._go_up),
            ("Refresh", self._refresh_file_list),
            ("Hidden", self._toggle_hidden),
            ("Terminal", self._open_terminal_here),
        ]:
            button = Gtk.Button(label=label)
            button.add_css_class("flat")
            button.connect("clicked", callback)
            header.pack_end(button)
        toolbar.add_top_bar(header)
        toolbar.set_content(self._build_content())
        self.set_content(toolbar)

    def _build_content(self):
        split = Adw.NavigationSplitView()
        split.set_sidebar(Adw.NavigationPage(title="Places", child=self._build_places()))
        split.set_content(Adw.NavigationPage(title="Files", child=self._build_file_list()))
        return split

    def _build_places(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        box.set_margin_top(12)
        box.set_margin_bottom(12)
        box.set_margin_start(12)
        box.set_margin_end(12)

        for label, path in [
            ("Home", os.path.expanduser("~")),
            ("Desktop", os.path.expanduser("~/Desktop")),
            ("Documents", os.path.expanduser("~/Documents")),
            ("Downloads", os.path.expanduser("~/Downloads")),
            ("Filesystem", "/"),
        ]:
            button = Gtk.Button(label=label)
            button.add_css_class("flat")
            button.set_halign(Gtk.Align.FILL)
            button.connect("clicked", self._on_place_clicked, path)
            box.append(button)

        return box

    def _build_file_list(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        box.set_margin_top(12)
        box.set_margin_bottom(12)
        box.set_margin_start(12)
        box.set_margin_end(12)

        self.search_entry = Gtk.SearchEntry()
        self.search_entry.set_placeholder_text("Search this folder")
        self.search_entry.connect("search-changed", self._on_search_changed)
        box.append(self.search_entry)

        self.list_box = Gtk.ListBox()
        self.list_box.add_css_class("boxed-list")
        self.list_box.connect("row-activated", self._on_row_activated)
        scroller = Gtk.ScrolledWindow()
        scroller.set_child(self.list_box)
        scroller.set_vexpand(True)
        box.append(scroller)

        self._refresh_file_list()
        return box

    def _on_place_clicked(self, _button, path):
        self.current_path = path
        self._refresh_file_list()

    def _on_row_activated(self, _list_box, row):
        path = getattr(row, "path", None)
        if path and os.path.isdir(path):
            self.current_path = path
            self._refresh_file_list()
        elif path:
            self._open_file(path)

    def _on_search_changed(self, _entry):
        self._refresh_file_list()

    def _go_up(self, _button):
        parent = os.path.dirname(os.path.abspath(self.current_path))
        if parent and parent != self.current_path:
            self.current_path = parent
            self._refresh_file_list()

    def _toggle_hidden(self, _button):
        self.show_hidden = not self.show_hidden
        self._refresh_file_list()

    def _open_terminal_here(self, _button):
        command = f"cd {self._shell_quote(self.current_path)}; exec bash"
        try:
            subprocess.Popen(["zenith-terminal", "--command", command])
        except OSError:
            pass

    def _open_file(self, path):
        try:
            subprocess.Popen(["gio", "open", path])
        except OSError:
            pass

    def _refresh_file_list(self, *_args):
        while (child := self.list_box.get_first_child()) is not None:
            self.list_box.remove(child)

        self.title_widget.set_subtitle(self.current_path)
        query = self.search_entry.get_text().casefold().strip() if hasattr(self, "search_entry") else ""

        try:
            names = sorted(os.listdir(self.current_path))
        except OSError as exc:
            row = Adw.ActionRow(title="Unable to open folder", subtitle=str(exc))
            self.list_box.append(row)
            return

        visible = []
        for name in names:
            if not self.show_hidden and name.startswith("."):
                continue
            if query and query not in name.casefold():
                continue
            visible.append(name)

        count = len(visible)
        summary = Adw.ActionRow(
            title=f"{count} item{'s' if count != 1 else ''}",
            subtitle="Hidden files shown" if self.show_hidden else "Hidden files hidden",
        )
        summary.set_activatable(False)
        self.list_box.append(summary)

        for name in visible[:300]:
            path = os.path.join(self.current_path, name)
            row = Adw.ActionRow(title=name, subtitle=self._metadata(path))
            row.path = path
            row.set_activatable(True)
            self.list_box.append(row)

        if count > 300:
            self.list_box.append(Adw.ActionRow(
                title="List truncated",
                subtitle=f"Showing 300 of {count} matching items",
            ))

    def _metadata(self, path):
        try:
            stat = os.stat(path)
        except OSError:
            return "Unavailable"
        modified = datetime.fromtimestamp(stat.st_mtime).strftime("%Y-%m-%d %H:%M")
        if os.path.isdir(path):
            try:
                count = len(os.listdir(path))
                return f"Folder - {count} items - modified {modified}"
            except OSError:
                return f"Folder - modified {modified}"
        return f"File - {self._format_size(stat.st_size)} - modified {modified}"

    def _format_size(self, size):
        units = ["B", "KB", "MB", "GB"]
        value = float(size)
        for unit in units:
            if value < 1024 or unit == units[-1]:
                if unit == "B":
                    return f"{int(value)} {unit}"
                return f"{value:.1f} {unit}"
            value /= 1024
        return f"{size} B"

    def _shell_quote(self, value):
        return "'" + value.replace("'", "'\"'\"'") + "'"


class FilesApp(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id="os.zenith.Files",
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )

    def do_activate(self):
        window = self.props.active_window
        if window is None:
            window = FilesWindow(self)
        window.present()


def main():
    app = FilesApp()
    return app.run(None)


if __name__ == "__main__":
    raise SystemExit(main())
