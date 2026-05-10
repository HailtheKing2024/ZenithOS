#include "zenith/user.h"

static void redraw(void)
{
    (void)z_ui_send(Z_UI_MSG_CLEAR, 0, 0, 0, 0, 0);
    (void)z_ui_send(Z_UI_MSG_SET_LINE, 0, 0, 0, 0, "TERMINAL");
    (void)z_ui_send(Z_UI_MSG_SET_LINE, 0, 1, 0, 0, "SETTINGS");
    (void)z_ui_send(Z_UI_MSG_SET_LINE, 0, 2, 0, 0, "FILES");
    (void)z_ui_send(Z_UI_MSG_SET_LINE, 0, 3, 0, 0, "PACKAGES");
}

static int in_rect(uint64_t px, uint64_t py, uint64_t x, uint64_t y, uint64_t w, uint64_t h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void handle_mouse(uint64_t x, uint64_t y)
{
    if (in_rect(x, y, 120, 210, 150, 88)) {
        (void)z_ui_send(Z_UI_MSG_COMMAND, 0, Z_UI_CMD_SHOW_TERMINAL, 0, 0, "TERMINAL");
    } else if (in_rect(x, y, 300, 210, 150, 88)) {
        (void)z_ui_send(Z_UI_MSG_COMMAND, 0, Z_UI_CMD_SHOW_SETTINGS, 0, 0, "SETTINGS");
    } else if (in_rect(x, y, 480, 210, 150, 88)) {
        (void)z_ui_send(Z_UI_MSG_NOTIFY, 0, Z_UI_CMD_NOTIFY, 0, 0, "FILES APP NOT BUILT YET");
    } else if (in_rect(x, y, 660, 210, 150, 88)) {
        (void)z_ui_send(Z_UI_MSG_NOTIFY, 0, Z_UI_CMD_NOTIFY, 0, 0, "PACKAGE UI PLANNED");
    }
}

static void handle_key(uint64_t key)
{
    if (key == 'T') {
        (void)z_ui_send(Z_UI_MSG_COMMAND, 0, Z_UI_CMD_SHOW_TERMINAL, 0, 0, "TERMINAL");
    } else if (key == 'S') {
        (void)z_ui_send(Z_UI_MSG_COMMAND, 0, Z_UI_CMD_SHOW_SETTINGS, 0, 0, "SETTINGS");
    }
}

int main(const char *arg)
{
    (void)arg;

    (void)z_ui_send(Z_UI_MSG_REGISTER, 0, Z_UI_ROLE_LAUNCHER, 0, 0, "LAUNCHER");
    redraw();

    for (;;) {
        struct z_ui_message message;
        if (z_ui_recv(&message) == 0) {
            if (message.type == Z_UI_MSG_INPUT_MOUSE) {
                handle_mouse(message.a, message.b);
            } else if (message.type == Z_UI_MSG_INPUT_KEY) {
                handle_key(message.a);
            }
        } else {
            z_sleep_ticks(1);
        }
    }
}
