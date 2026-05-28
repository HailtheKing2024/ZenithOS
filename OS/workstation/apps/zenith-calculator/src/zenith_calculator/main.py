import operator
import sys

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gio, Gtk


class CalculatorWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="Zenith Calculator")
        self.set_default_size(480, 640)
        self._expression = "0"
        self._reset_next = False

        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title="Calculator"))
        toolbar.add_top_bar(header)
        toolbar.set_content(self._build_content())
        self.set_content(toolbar)

        self._key_controller = Gtk.EventControllerKey()
        self._key_controller.connect("key-pressed", self._on_key_pressed)
        self.add_controller(self._key_controller)

    def _build_content(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        box.add_css_class("view")

        # Display
        display_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        display_box.set_margin_top(24)
        display_box.set_margin_bottom(12)
        display_box.set_margin_start(16)
        display_box.set_margin_end(16)

        self.display_label = Gtk.Label(label="0")
        self.display_label.add_css_class("title-1")
        self.display_label.set_halign(Gtk.Align.END)
        self.display_label.set_ellipsize(3)
        display_box.append(self.display_label)

        self.sub_display = Gtk.Label(label="")
        self.sub_display.add_css_class("caption")
        self.sub_display.set_halign(Gtk.Align.END)
        self.sub_display.add_css_class("dim-label")
        display_box.append(self.sub_display)
        box.append(display_box)

        # Separator
        separator = Gtk.Separator()
        box.append(separator)

        # Buttons
        buttons = self._build_buttons()
        box.append(buttons)

        return box

    def _build_buttons(self):
        grid = Gtk.Grid()
        grid.set_column_homogeneous(True)
        grid.set_row_homogeneous(True)
        grid.set_margin_top(12)
        grid.set_margin_bottom(12)
        grid.set_margin_start(12)
        grid.set_margin_end(12)
        grid.set_row_spacing(6)
        grid.set_column_spacing(6)
        grid.set_halign(Gtk.Align.FILL)
        grid.set_valign(Gtk.Align.FILL)
        grid.set_vexpand(True)

        buttons = [
            ("C", 0, 0, "clear", "destructive-action"),
            ("⌫", 1, 0, "backspace", None),
            ("^", 2, 0, "op", None),
            ("/", 3, 0, "op", None),
            ("7", 0, 1, "digit", None),
            ("8", 1, 1, "digit", None),
            ("9", 2, 1, "digit", None),
            ("*", 3, 1, "op", None),
            ("4", 0, 2, "digit", None),
            ("5", 1, 2, "digit", None),
            ("6", 2, 2, "digit", None),
            ("-", 3, 2, "op", None),
            ("1", 0, 3, "digit", None),
            ("2", 1, 3, "digit", None),
            ("3", 2, 3, "digit", None),
            ("+", 3, 3, "op", None),
            ("0", 0, 4, "digit", None),
            (".", 1, 4, "dot", None),
            ("=", 2, 4, "equals", "suggested-action"),
            ("%", 3, 4, "op", None),
        ]

        for btn_text, col, row, action, css in buttons:
            button = Gtk.Button(label=btn_text)
            button.set_hexpand(True)
            button.set_vexpand(True)
            if css:
                button.add_css_class(css)
            button.connect("clicked", self._on_button_clicked, btn_text, action)
            grid.attach(button, col, row, 1, 1)

        return grid

    def _on_button_clicked(self, _button, text, action):
        if self._reset_next and action not in ("equals", "clear"):
            self._expression = "0"
            self._reset_next = False

        if action == "clear":
            self._expression = "0"
            self._reset_next = False
        elif action == "backspace":
            if self._expression == "0":
                return
            self._expression = self._expression[:-1] or "0"
        elif action == "digit":
            if self._expression == "0":
                self._expression = text
            else:
                self._expression += text
        elif action == "dot":
            if text not in self._expression.rsplit("+", 1)[-1].rsplit("-", 1)[-1].rsplit("*", 1)[-1].rsplit("/", 1)[-1].rsplit("^", 1)[-1]:
                self._expression += text
        elif action == "op":
            if self._expression and self._expression[-1] in "+-*/^":
                self._expression = self._expression[:-1] + text
            else:
                self._expression += text
        elif action == "equals":
            self._evaluate()
            return

        self._update_display()

    def _on_key_pressed(self, _controller, keyval, _keycode, _state):
        digits = "0123456789"
        if 32 < keyval < 127 and chr(keyval) in digits:
            self._on_button_clicked(None, chr(keyval), "digit")
            return True
        if keyval in (46,):  # .
            self._on_button_clicked(None, ".", "dot")
            return True
        ops = {43: "+", 45: "-", 42: "*", 47: "/", 37: "%", 94: "^"}
        if keyval in ops:
            self._on_button_clicked(None, ops[keyval], "op")
            return True
        if keyval in (65293, 65421):  # Return / Enter
            self._on_button_clicked(None, "=", "equals")
            return True
        if keyval == 65288:  # Backspace
            self._on_button_clicked(None, "⌫", "backspace")
            return True
        if keyval in (65535, 65307):  # Delete / Escape
            self._on_button_clicked(None, "C", "clear")
            return True
        return False

    def _update_display(self):
        self.display_label.set_label(self._expression)

    def _evaluate(self):
        try:
            expr = self._expression.replace("^", "**")
            expr = expr.replace("%", "/100")
            result = eval(expr, {"__builtins__": {}}, {})
            self.sub_display.set_label(f"= {self._expression}")
            if isinstance(result, float):
                if result == int(result):
                    result = int(result)
            self._expression = str(result)
            self._reset_next = True
            self._update_display()
        except Exception:
            self.display_label.set_label("Error")
            self.sub_display.set_label(f"= {self._expression}")
            self._expression = "0"
            self._reset_next = True


class CalculatorApp(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id="os.zenith.Calculator",
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )

    def do_activate(self):
        window = self.props.active_window
        if window is None:
            window = CalculatorWindow(self)
        window.present()


def main():
    app = CalculatorApp()
    return app.run(None)


if __name__ == "__main__":
    raise SystemExit(main())
