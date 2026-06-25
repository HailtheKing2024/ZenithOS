import os
import subprocess
from datetime import datetime

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gio, Gtk

try:
    from zenith_files.operations import copy_path, mounted_volumes, move_path, trash_path
except ImportError:
    from operations import copy_path, mounted_volumes, move_path, trash_path


class FilesWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="Zenith Files")
        self.set_default_size(1040, 700)
        self.current_path = os.path.expanduser("~")
        self.selected_path = None
        self.clipboard_action = None
        self.show_hidden = False

        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        self.title_widget = Adw.WindowTitle(title="Files", subtitle=self.current_path)
        header.set_title_widget(self.title_widget)
        for label, callback in [
            ("Up", self._go_up),
            ("Refresh", self._refresh_file_list),
            ("Hidden", self._toggle_hidden),
            ("Copy", self._copy_selected),
            ("Move", self._move_selected),
            ("Paste", self._paste_clipboard),
            ("Trash", self._trash_selected),
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

        for label, path, source, fs_type in mounted_volumes():
            button = Gtk.Button(label=f"{label} ({fs_type})")
            button.add_css_class("flat")
            button.set_tooltip_text(f"{source} mounted at {path}")
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
        self.list_box.set_selection_mode(Gtk.SelectionMode.SINGLE)
        self.list_box.connect("row-selected", self._on_row_selected)
        self.list_box.connect("row-activated", self._on_row_activated)
        scroller = Gtk.ScrolledWindow()
        scroller.set_child(self.list_box)
        scroller.set_vexpand(True)
        box.append(scroller)

        self.status_label = Gtk.Label(label="Select an item to copy, move, or trash it.")
        self.status_label.set_halign(Gtk.Align.START)
        self.status_label.set_wrap(True)
        box.append(self.status_label)

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

    def _on_row_selected(self, _list_box, row):
        self.selected_path = getattr(row, "path", None) if row else None
        if self.selected_path:
            self._set_status(f"Selected {self.selected_path}")

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

    def _copy_selected(self, _button):
        if not self.selected_path:
            self._set_status("Select an item to copy first.")
            return
        self.clipboard_action = ("copy", self.selected_path)
        self._set_status(f"Copy ready: {os.path.basename(self.selected_path)}")

    def _move_selected(self, _button):
        if not self.selected_path:
            self._set_status("Select an item to move first.")
            return
        self.clipboard_action = ("move", self.selected_path)
        self._set_status(f"Move Here ready: {os.path.basename(self.selected_path)}")

    def _paste_clipboard(self, _button):
        if not self.clipboard_action:
            self._set_status("Copy or move an item before using Paste.")
            return
        action, source = self.clipboard_action
        try:
            if action == "copy":
                destination = copy_path(source, self.current_path)
            else:
                destination = move_path(source, self.current_path)
                self.clipboard_action = None
        except (OSError, ValueError) as exc:
            self._set_status(f"{action.capitalize()} failed: {exc}")
            return
        self._set_status(f"{action.capitalize()} complete: {destination}")
        self._refresh_file_list()

    def _trash_selected(self, _button):
        if not self.selected_path:
            self._set_status("Select an item to trash first.")
            return
        try:
            trashed = trash_path(self.selected_path)
        except OSError as exc:
            self._set_status(f"Trash failed: {exc}")
            return
        self.selected_path = None
        self._set_status(f"Moved to Trash: {trashed}")
        self._refresh_file_list()

    def _refresh_file_list(self, *_args):
        while (child := self.list_box.get_first_child()) is not None:
            self.list_box.remove(child)

        self.selected_path = None
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
        summary.set_selectable(False)
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

    def _set_status(self, message):
        if hasattr(self, "status_label"):
            self.status_label.set_text(message)


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
