#include "zenith/user.h"

static uint64_t network_on;
static uint64_t compact_mode;

static void send_line(uint64_t row, const char *text)
{
    (void)z_ui_send(Z_UI_MSG_SET_LINE, 0, row, 0, 0, text);
}

static void redraw(void)
{
    (void)z_ui_send(Z_UI_MSG_CLEAR, 0, 0, 0, 0, 0);
    send_line(0, "SYSTEM SETTINGS");
    send_line(1, network_on != 0 ? "NETWORK    ON" : "NETWORK    OFFLINE");
    send_line(2, "AUDIO      PIPEWIRE TARGET");
    send_line(3, "THEME      ADWAITA TOKENS");
    send_line(4, compact_mode != 0 ? "LAYOUT     COMPACT" : "LAYOUT     DEFAULT");
    send_line(5, "PACKAGES   APT PLUS FLATPAK PLAN");
    send_line(7, "CLICK ROWS TO TOGGLE SETTINGS");
}

static void handle_mouse(uint64_t x, uint64_t y)
{
    (void)x;

    if (y >= 24 && y < 58) {
        network_on = network_on == 0 ? 1 : 0;
        (void)z_ui_send(Z_UI_MSG_NOTIFY, 0, Z_UI_CMD_NOTIFY, 0, 0, "NETWORK TOGGLED");
    } else if (y >= 108 && y < 142) {
        compact_mode = compact_mode == 0 ? 1 : 0;
        (void)z_ui_send(Z_UI_MSG_NOTIFY, 0, Z_UI_CMD_NOTIFY, 0, 0, "LAYOUT TOGGLED");
    } else if (y >= 136 && y < 174) {
        (void)z_ui_send(Z_UI_MSG_NOTIFY, 0, Z_UI_CMD_NOTIFY, 0, 0, "PACKAGE UI PLANNED");
    }

    redraw();
}

static void handle_key(uint64_t key)
{
    if (key == '\n' || key == '\r' || key == ' ') {
        network_on = network_on == 0 ? 1 : 0;
        (void)z_ui_send(Z_UI_MSG_NOTIFY, 0, Z_UI_CMD_NOTIFY, 0, 0, "NETWORK TOGGLED");
        redraw();
    }
}

int main(const char *arg)
{
    (void)arg;

    (void)z_ui_send(Z_UI_MSG_REGISTER, 0, Z_UI_ROLE_SETTINGS, 318, 316, "SETTINGS");
    redraw();

    for (;;) {
        struct z_ui_message message;
        if (z_ui_recv(&message) == 0) {
            if (message.type == Z_UI_MSG_INPUT_MOUSE) {
                handle_mouse(message.a, message.b);
            } else if (message.type == Z_UI_MSG_INPUT_KEY) {
                handle_key(message.a);
            } else if (message.type == Z_UI_MSG_CLOSE) {
                z_exit(0);
            }
        } else {
            z_sleep_ticks(1);
        }
    }
}
