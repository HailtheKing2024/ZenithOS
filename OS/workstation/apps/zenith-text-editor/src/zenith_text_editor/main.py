import os
import sys

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gio, GLib, Gtk


class TextEditorWindow(Adw.ApplicationWindow):
    def __init__(self, app, filepath=None):
        super().__init__(application=app, title="Zenith Text Editor")
        self.set_default_size(1040, 700)
        self._filepath = None
        self._dirty = False

        self._toast_overlay = Adw.ToastOverlay()

        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        self._title = Adw.WindowTitle(title="Text Editor")
        header.set_title_widget(self._title)

        for label, callback in [
            ("New", self._new_file),
            ("Open", self._open_file),
            ("Save", self._save_file),
            ("Save As", self._save_as),
        ]:
            button = Gtk.Button(label=label)
            button.add_css_class("flat")
            button.connect("clicked", callback)
            header.pack_start(button)

        toolbar.add_top_bar(header)
        toolbar.set_content(self._build_content())
        self._toast_overlay.set_child(toolbar)
        self.set_content(self._toast_overlay)

        if filepath and os.path.isfile(filepath):
            self._load_file(filepath)

    def _build_content(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        box.add_css_class("view")

        self.text_view = Gtk.TextView()
        self.text_view.set_monospace(True)
        self.buffer = self.text_view.get_buffer()
        self.buffer.connect("changed", self._on_buffer_changed)
        self.text_view.set_vexpand(True)
        self.text_view.set_hexpand(True)
        self.text_view.set_margin_start(12)
        self.text_view.set_margin_end(12)
        self.text_view.set_margin_top(12)
        self.text_view.set_margin_bottom(12)

        scroller = Gtk.ScrolledWindow()
        scroller.set_child(self.text_view)
        scroller.set_vexpand(True)
        scroller.set_hexpand(True)
        box.append(scroller)

        # Status bar
        self.status_label = Gtk.Label(label="Untitled document")
        self.status_label.add_css_class("caption")
        self.status_label.set_halign(Gtk.Align.START)
        self.status_label.set_margin_start(12)
        self.status_label.set_margin_end(12)
        self.status_label.set_margin_bottom(6)
        self.status_label.set_margin_top(6)
        self.status_label.add_css_class("dim-label")
        box.append(Gtk.Separator())
        box.append(self.status_label)

        return box

    def _on_buffer_changed(self, _buf):
        self._dirty = True
        self._update_title()

    def _update_title(self):
        name = self._filepath or "Untitled document"
        base = os.path.basename(name) if self._filepath else name
        dirty = " *" if self._dirty else ""
        self._title.set_subtitle(f"{base}{dirty}")
        self.status_label.set_label(f"{base}{dirty}")

    def _new_file(self, _button):
        self._maybe_discard(self._reset_document)

    def _reset_document(self):
        self.buffer.set_text("", 0)
        self._filepath = None
        self._dirty = False
        self._update_title()

    def _open_file(self, _button):
        self._maybe_discard(self._show_open_dialog)

    def _show_open_dialog(self):
        dialog = Gtk.FileDialog()
        dialog.set_title("Open File")
        dialog.open(self, None, self._on_open_dialog_response)

    def _on_open_dialog_response(self, dialog, result):
        try:
            gfile = dialog.open_finish(result)
        except GLib.Error:
            return
        if gfile:
            path = gfile.get_path()
            if path:
                self._load_file(path)

    def _load_file(self, filepath):
        try:
            with open(filepath, "r", encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError as exc:
            self._show_toast(f"Failed to open file: {exc}")
            return
        self.buffer.set_text(text, len(text.encode("utf-8")))
        self._filepath = filepath
        self._dirty = False
        self._update_title()

    def _save_file(self, _button):
        if self._filepath:
            self._do_save(self._filepath)
        else:
            self._show_save_dialog()

    def _save_as(self, _button):
        self._show_save_dialog()

    def _show_save_dialog(self):
        dialog = Gtk.FileDialog()
        dialog.set_title("Save As")
        dialog.save(self, None, self._on_save_dialog_response)

    def _on_save_dialog_response(self, dialog, result):
        try:
            gfile = dialog.save_finish(result)
        except GLib.Error:
            return
        if gfile:
            path = gfile.get_path()
            if path:
                self._do_save(path)

    def _do_save(self, filepath):
        text = self.buffer.get_text(
            self.buffer.get_start_iter(),
            self.buffer.get_end_iter(),
            False,
        )
        try:
            with open(filepath, "w", encoding="utf-8") as f:
                f.write(text)
        except OSError as exc:
            self._show_toast(f"Failed to save file: {exc}")
            return
        self._filepath = filepath
        self._dirty = False
        self._update_title()
        self._show_toast("File saved")

    def _maybe_discard(self, callback):
        if not self._dirty:
            callback()
            return

        dialog = Adw.AlertDialog()
        dialog.set_heading("Unsaved Changes")
        dialog.set_body("The current document has unsaved changes. Discard them?")
        dialog.add_response("cancel", "Cancel")
        dialog.add_response("discard", "Discard")
        dialog.set_default_response("cancel")
        dialog.set_response_appearance("discard", Adw.ResponseAppearance.DESTRUCTIVE)

        def on_response(_dialog, response):
            if response == "discard":
                callback()

        dialog.connect("response", on_response)
        dialog.present(self)

    def _show_toast(self, message):
        toast = Adw.Toast(title=message)
        self._toast_overlay.add_toast(toast)

    def do_close_request(self):
        if self._dirty:
            self._maybe_discard(self._confirm_close)
            return True
        self.destroy()
        return True

    def _confirm_close(self):
        self.destroy()


class TextEditorApp(Adw.Application):
    def __init__(self, filepath=None):
        super().__init__(
            application_id="os.zenith.TextEditor",
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )
        self._filepath = filepath
        self._setup_actions()

    def _setup_actions(self):
        actions = [
            ("new", self._action_new, "<primary>n"),
            ("open", self._action_open, "<primary>o"),
            ("save", self._action_save, "<primary>s"),
            ("save-as", self._action_save_as, "<primary><shift>s"),
        ]
        for name, callback, accel in actions:
            action = Gio.SimpleAction(name=name)
            action.connect("activate", callback)
            self.add_action(action)
            self.set_accels_for_action(f"app.{name}", [accel])

    def _action_new(self, _action, _param):
        win = self.props.active_window
        if win:
            win._maybe_discard(win._reset_document)

    def _action_open(self, _action, _param):
        win = self.props.active_window
        if win:
            win._maybe_discard(win._show_open_dialog)

    def _action_save(self, _action, _param):
        win = self.props.active_window
        if win:
            win._save_file(None)

    def _action_save_as(self, _action, _param):
        win = self.props.active_window
        if win:
            win._save_as(None)

    def do_activate(self):
        window = self.props.active_window
        if window is None:
            window = TextEditorWindow(self, filepath=self._filepath)
        window.present()


def main(argv=None):
    filepath = None
    if argv and len(argv) > 1:
        filepath = argv[1]
    app = TextEditorApp(filepath=filepath)
    return app.run(None)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
