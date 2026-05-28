import sys
from datetime import datetime

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gio, GLib, Gtk

COMMON_TIMEZONES = [
    "UTC",
    "US/Eastern",
    "US/Central",
    "US/Mountain",
    "US/Pacific",
    "Europe/London",
    "Europe/Paris",
    "Europe/Berlin",
    "Europe/Moscow",
    "Asia/Dubai",
    "Asia/Kolkata",
    "Asia/Shanghai",
    "Asia/Tokyo",
    "Australia/Sydney",
    "Pacific/Auckland",
]


class ClockWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="Zenith Clock")
        self.set_default_size(480, 400)
        self._timeout = None
        self._timezone = None

        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title="Clock"))
        toolbar.add_top_bar(header)
        toolbar.set_content(self._build_content())
        self.set_content(toolbar)
        self._start_timer()

    def _build_content(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        box.add_css_class("view")

        content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
        content.set_margin_top(48)
        content.set_margin_bottom(48)
        content.set_margin_start(24)
        content.set_margin_end(24)
        content.set_vexpand(True)
        content.set_halign(Gtk.Align.FILL)
        content.set_valign(Gtk.Align.CENTER)
        box.append(content)

        # Time display
        self.time_label = Gtk.Label(label="--:--:--")
        self.time_label.add_css_class("title-1")
        self.time_label.add_css_class("monospace")
        self.time_label.set_halign(Gtk.Align.CENTER)
        content.append(self.time_label)

        # Date display
        self.date_label = Gtk.Label(label="--")
        self.date_label.add_css_class("body")
        self.date_label.set_halign(Gtk.Align.CENTER)
        self.date_label.add_css_class("dim-label")
        content.append(self.date_label)

        # Separator
        sep = Gtk.Separator()
        sep.set_margin_top(16)
        sep.set_margin_bottom(16)
        content.append(sep)

        # Timezone selector
        combo_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        combo_box.set_halign(Gtk.Align.CENTER)

        tz_label = Gtk.Label(label="Timezone:")
        tz_label.add_css_class("body")
        combo_box.append(tz_label)

        self.tz_combo = Gtk.DropDown()
        self.tz_store = Gtk.StringList()
        for tz in COMMON_TIMEZONES:
            self.tz_store.append(tz)
        self.tz_combo.set_model(self.tz_store)
        self.tz_combo.connect("notify::selected", self._on_timezone_changed)
        combo_box.append(self.tz_combo)

        content.append(combo_box)

        # Seconds toggle
        toggle_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        toggle_box.set_halign(Gtk.Align.CENTER)
        self.seconds_switch = Gtk.Switch()
        self.seconds_switch.set_active(True)
        self.seconds_switch.set_valign(Gtk.Align.CENTER)
        seconds_label = Gtk.Label(label="Show seconds")
        toggle_box.append(seconds_label)
        toggle_box.append(self.seconds_switch)
        content.append(toggle_box)

        self.seconds_switch.connect("notify::active", self._update_time_cb)

        self._update_time()
        return box

    def _tz_name(self):
        index = self.tz_combo.get_selected()
        return COMMON_TIMEZONES[index] if 0 <= index < len(COMMON_TIMEZONES) else "UTC"

    def _start_timer(self):
        if self._timeout:
            GLib.source_remove(self._timeout)
        self._timeout = GLib.timeout_add(1000, self._update_time_cb)

    def _update_time_cb(self, *args):
        self._update_time()
        return True

    def _update_time(self):
        try:
            from zoneinfo import ZoneInfo
        except ImportError:
            self.time_label.set_label(datetime.now().strftime("%I:%M:%S %p"))
            self.date_label.set_label(datetime.now().strftime("%A, %B %d, %Y"))
            return True

        tz_name = self._tz_name()
        try:
            tz = ZoneInfo(tz_name)
            now = datetime.now(tz)
        except Exception:
            now = datetime.now()

        if self.seconds_switch.get_active():
            self.time_label.set_label(now.strftime("%I:%M:%S %p"))
        else:
            self.time_label.set_label(now.strftime("%I:%M %p"))
        self.date_label.set_label(now.strftime("%A, %B %d, %Y"))

        return True

    def _on_timezone_changed(self, *args):
        self._update_time()

    def do_close_request(self):
        if self._timeout:
            GLib.source_remove(self._timeout)
            self._timeout = None
        self.destroy()
        return True


class ClockApp(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id="os.zenith.Clock",
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS,
        )

    def do_activate(self):
        window = self.props.active_window
        if window is None:
            window = ClockWindow(self)
        window.present()


def main():
    app = ClockApp()
    return app.run(None)


if __name__ == "__main__":
    raise SystemExit(main())
