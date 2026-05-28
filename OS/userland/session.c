#include "zenith/user.h"

#include <stdbool.h>

enum {
    SCREEN_W = 1280,
    SCREEN_H = 800,
    TOP_BAR_H = 42,
    MAX_WINDOWS = 6,
    MAX_LINES = 12,
    SESSION_HISTORY_MAX = 8,
    TITLE_H = 36,
    MODE_NORMAL = 0,
    MODE_OVERVIEW = 1
};

#define FOCUS_NONE UINT64_MAX
#define DRAG_NONE UINT64_MAX

struct window {
    uint64_t pid;
    uint64_t role;
    uint64_t x;
    uint64_t y;
    uint64_t w;
    uint64_t h;
    uint64_t line_count;
    bool used;
    bool visible;
    bool minimized;
    char title[Z_UI_TEXT_CAP];
    char lines[MAX_LINES][Z_UI_TEXT_CAP];
};

struct session_state {
    struct window windows[MAX_WINDOWS];
    uint64_t focus;
    uint64_t drag;
    uint64_t cursor_x;
    uint64_t cursor_y;
    uint64_t mouse_buttons;
    uint64_t mode;
    uint64_t terminal_history_count;
    uint64_t terminal_history_next;
    char terminal_history[SESSION_HISTORY_MAX][Z_UI_TEXT_CAP];
    char status[Z_UI_TEXT_CAP];
};

static const uint32_t color_wallpaper = 0x24282f;
static const uint32_t color_top = 0x111418;
static const uint32_t color_panel = 0x2b3138;
static const uint32_t color_panel_hi = 0x38414a;
static const uint32_t color_text = 0xf4f6f7;
static const uint32_t color_muted = 0xaeb8c2;
static const uint32_t color_accent = 0x4aa78a;
static const uint32_t color_focus = 0x6da7ff;
static const uint32_t color_shadow = 0x111418;
static const uint32_t color_surface = 0x171b20;
static const uint32_t color_warn = 0xe2b35f;

static const char terminal_path[] = "/bin/terminal";
static const char settings_path[] = "/bin/settings";
static const char launcher_path[] = "/bin/launcher";

static void copy_text(char *dest, uint64_t dest_size, const char *src)
{
    if (dest_size == 0) {
        return;
    }

    if (src == 0) {
        dest[0] = '\0';
        return;
    }

    uint64_t index = 0;
    while (index + 1 < dest_size && src[index] != '\0') {
        dest[index] = src[index];
        index++;
    }

    dest[index] = '\0';
}

static void set_status(struct session_state *state, const char *text)
{
    copy_text(state->status, sizeof(state->status), text);
}

static uint64_t string_length(const char *value)
{
    uint64_t length = 0;

    while (value[length] != '\0') {
        length++;
    }

    return length;
}

static bool string_starts_with(const char *value, const char *prefix)
{
    while (*prefix != '\0') {
        if (*value++ != *prefix++) {
            return false;
        }
    }

    return true;
}

static void append_text(char *dest, uint64_t dest_size, const char *src)
{
    uint64_t length = string_length(dest);
    uint64_t index = 0;

    while (length + 1 < dest_size && src[index] != '\0') {
        dest[length++] = src[index++];
    }

    dest[length] = '\0';
}

static uint64_t terminal_history_slot(const struct session_state *state, uint64_t view)
{
    uint64_t first = state->terminal_history_count < SESSION_HISTORY_MAX ?
        0 :
        state->terminal_history_next;

    return (first + view) % SESSION_HISTORY_MAX;
}

static void terminal_history_add(struct session_state *state, const char *line)
{
    if (line[0] == '\0') {
        return;
    }

    copy_text(state->terminal_history[state->terminal_history_next],
              sizeof(state->terminal_history[state->terminal_history_next]),
              line);
    state->terminal_history_next = (state->terminal_history_next + 1) % SESSION_HISTORY_MAX;
    if (state->terminal_history_count < SESSION_HISTORY_MAX) {
        state->terminal_history_count++;
    }
}

static void send_history_line(uint64_t target, const char *line)
{
    char payload[Z_UI_TEXT_CAP];

    copy_text(payload, sizeof(payload), "HIST ");
    append_text(payload, sizeof(payload), line);
    (void)z_ui_send(Z_UI_MSG_COMMAND,
                    target,
                    Z_UI_CMD_BRIDGE_DATA,
                    0,
                    string_length(payload),
                    payload);
}

static void send_terminal_history(struct session_state *state, uint64_t target)
{
    for (uint64_t i = 0; i < state->terminal_history_count; i++) {
        send_history_line(target, state->terminal_history[terminal_history_slot(state, i)]);
    }
}

static int64_t sign_extend16(uint64_t value)
{
    int64_t result = (int64_t)(value & UINT64_C(0xffff));
    if ((value & UINT64_C(0x8000)) != 0) {
        result -= INT64_C(0x10000);
    }

    return result;
}

static uint64_t clamp_add(uint64_t value, int64_t delta, uint64_t min, uint64_t max)
{
    int64_t next = (int64_t)value + delta;
    if (next < (int64_t)min) {
        return min;
    }

    if (next > (int64_t)max) {
        return max;
    }

    return (uint64_t)next;
}

static bool point_in(uint64_t px, uint64_t py, uint64_t x, uint64_t y, uint64_t w, uint64_t h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

static struct window *find_window_by_pid(struct session_state *state, uint64_t pid)
{
    for (uint64_t i = 0; i < MAX_WINDOWS; i++) {
        if (state->windows[i].used && state->windows[i].pid == pid) {
            return &state->windows[i];
        }
    }

    return 0;
}

static uint64_t find_window_index_by_pid(struct session_state *state, uint64_t pid)
{
    for (uint64_t i = 0; i < MAX_WINDOWS; i++) {
        if (state->windows[i].used && state->windows[i].pid == pid) {
            return i;
        }
    }

    return FOCUS_NONE;
}

static uint64_t find_window_index_by_role(struct session_state *state, uint64_t role)
{
    for (uint64_t i = 0; i < MAX_WINDOWS; i++) {
        if (state->windows[i].used && state->windows[i].role == role) {
            return i;
        }
    }

    return FOCUS_NONE;
}

static uint64_t add_window(struct session_state *state, const struct z_ui_message *message)
{
    uint64_t index = find_window_index_by_pid(state, message->source);
    if (index != FOCUS_NONE) {
        return index;
    }

    for (uint64_t i = 0; i < MAX_WINDOWS; i++) {
        if (!state->windows[i].used) {
            struct window *window = &state->windows[i];
            window->used = true;
            window->pid = message->source;
            window->role = message->a;
            window->line_count = 0;
            window->visible = message->a != Z_UI_ROLE_LAUNCHER;
            window->minimized = false;
            copy_text(window->title, sizeof(window->title), message->text);

            if (message->a == Z_UI_ROLE_TERMINAL) {
                window->x = 96;
                window->y = 112;
                window->w = 760;
                window->h = 472;
                state->focus = i;
            } else if (message->a == Z_UI_ROLE_SETTINGS) {
                window->x = 890;
                window->y = 112;
                window->w = 318;
                window->h = 316;
            } else {
                window->x = 0;
                window->y = 0;
                window->w = 0;
                window->h = 0;
            }

            if (message->b >= 240) {
                window->w = message->b;
            }
            if (message->c >= 180) {
                window->h = message->c;
            }

            set_status(state, "APP REGISTERED");
            return i;
        }
    }

    set_status(state, "WINDOW TABLE FULL");
    return FOCUS_NONE;
}

static void clear_window_lines(struct window *window)
{
    window->line_count = 0;
    for (uint64_t i = 0; i < MAX_LINES; i++) {
        window->lines[i][0] = '\0';
    }
}

static void set_window_line(struct window *window, uint64_t row, const char *text)
{
    if (row >= MAX_LINES) {
        return;
    }

    copy_text(window->lines[row], sizeof(window->lines[row]), text);
    if (window->line_count <= row) {
        window->line_count = row + 1;
    }
}

static void focus_role(struct session_state *state, uint64_t role)
{
    uint64_t index = find_window_index_by_role(state, role);
    if (index == FOCUS_NONE) {
        set_status(state, "APP NOT REGISTERED");
        return;
    }

    state->windows[index].visible = true;
    state->windows[index].minimized = false;
    state->focus = index;
    state->mode = MODE_NORMAL;
}

static void handle_command(struct session_state *state, const struct z_ui_message *message)
{
    if (message->a == Z_UI_CMD_SHOW_TERMINAL) {
        focus_role(state, Z_UI_ROLE_TERMINAL);
    } else if (message->a == Z_UI_CMD_SHOW_SETTINGS) {
        focus_role(state, Z_UI_ROLE_SETTINGS);
    } else if (message->a == Z_UI_CMD_SHOW_OVERVIEW) {
        state->mode = MODE_OVERVIEW;
        set_status(state, "OVERVIEW");
    } else if (message->a == Z_UI_CMD_NOTIFY) {
        set_status(state, message->text);
    } else if (message->a == Z_UI_CMD_BRIDGE_DATA) {
        if (string_starts_with(message->text, "BRG SETTINGS")) {
            focus_role(state, Z_UI_ROLE_SETTINGS);
        } else if (string_starts_with(message->text, "BRG OVERVIEW")) {
            state->mode = MODE_OVERVIEW;
            set_status(state, "OVERVIEW");
        } else if (string_starts_with(message->text, "BRG HIST_LOAD")) {
            send_terminal_history(state, message->source);
            set_status(state, "HISTORY LOADED");
        } else if (string_starts_with(message->text, "BRG HIST_PUSH ")) {
            terminal_history_add(state, message->text + 14);
            set_status(state, "HISTORY SAVED");
        } else {
            set_status(state, message->text);
        }
    }
}

static void process_ui_message(struct session_state *state, const struct z_ui_message *message)
{
    struct window *window = find_window_by_pid(state, message->source);

    if (message->type == Z_UI_MSG_REGISTER) {
        (void)add_window(state, message);
        return;
    }

    if (message->type == Z_UI_MSG_COMMAND) {
        handle_command(state, message);
        return;
    }

    if (message->type == Z_UI_MSG_NOTIFY) {
        set_status(state, message->text);
        return;
    }

    if (window == 0) {
        return;
    }

    if (message->type == Z_UI_MSG_CLEAR) {
        clear_window_lines(window);
    } else if (message->type == Z_UI_MSG_SET_LINE) {
        set_window_line(window, message->a, message->text);
    } else if (message->type == Z_UI_MSG_CLOSE) {
        window->visible = false;
    }
}

static void pump_ui_messages(struct session_state *state)
{
    for (uint64_t i = 0; i < 24; i++) {
        struct z_ui_message message;
        if (z_ui_recv(&message) != 0) {
            return;
        }

        process_ui_message(state, &message);
    }
}

static void send_key_to_focused(struct session_state *state, uint64_t key)
{
    if (state->focus == FOCUS_NONE || state->focus >= MAX_WINDOWS) {
        return;
    }

    struct window *window = &state->windows[state->focus];
    if (!window->used || !window->visible || window->minimized) {
        return;
    }

    (void)z_ui_send(Z_UI_MSG_INPUT_KEY, window->pid, key, 0, 0, 0);
}

static void send_mouse_to_window(struct window *window, uint64_t x, uint64_t y, uint64_t buttons)
{
    if (x < window->x + 14 || y < window->y + TITLE_H + 12) {
        return;
    }

    uint64_t rel_x = x - window->x - 14;
    uint64_t rel_y = y - window->y - TITLE_H - 12;
    (void)z_ui_send(Z_UI_MSG_INPUT_MOUSE, window->pid, rel_x, rel_y, buttons, 0);
}

static void focus_next_window(struct session_state *state)
{
    uint64_t start = state->focus == FOCUS_NONE ? 0 : state->focus + 1;

    for (uint64_t step = 0; step < MAX_WINDOWS; step++) {
        uint64_t index = (start + step) % MAX_WINDOWS;
        struct window *window = &state->windows[index];
        if (window->used &&
            window->role != Z_UI_ROLE_LAUNCHER &&
            window->visible &&
            !window->minimized) {
            state->focus = index;
            return;
        }
    }
}

static void draw_cursor(const struct session_state *state)
{
    uint32_t color = (state->mouse_buttons & Z_MOUSE_LEFT) != 0 ? color_accent : color_text;

    z_draw_rect(state->cursor_x, state->cursor_y, 4, 24, color);
    z_draw_rect(state->cursor_x + 4, state->cursor_y + 4, 4, 16, color);
    z_draw_rect(state->cursor_x + 8, state->cursor_y + 8, 4, 12, color);
    z_draw_rect(state->cursor_x + 12, state->cursor_y + 20, 12, 4, color);
}

static void draw_window(const struct window *window, bool focused)
{
    uint32_t border = focused ? color_focus : color_panel_hi;

    z_draw_rect(window->x + 8, window->y + 8, window->w, window->h, color_shadow);
    z_draw_rect(window->x, window->y, window->w, window->h, border);
    z_draw_rect(window->x + 2, window->y + 2, window->w - 4, window->h - 4, color_panel);
    z_draw_rect(window->x + 2, window->y + 2, window->w - 4, TITLE_H, border);
    z_draw_text(window->x + 16, window->y + 12, window->title, color_text, 2);

    z_draw_rect(window->x + window->w - 58, window->y + 11, 16, 16, color_warn);
    z_draw_rect(window->x + window->w - 30, window->y + 11, 16, 16, color_warn);
    z_draw_rect(window->x + 14, window->y + TITLE_H + 12,
                window->w - 28, window->h - TITLE_H - 26, color_surface);

    for (uint64_t row = 0; row < window->line_count && row < MAX_LINES; row++) {
        z_draw_text(window->x + 28,
                    window->y + TITLE_H + 28 + row * 28,
                    window->lines[row],
                    color_text,
                    2);
    }
}

static void draw_top_bar(const struct session_state *state)
{
    z_draw_rect(0, 0, SCREEN_W, TOP_BAR_H, color_top);
    z_draw_rect(0, TOP_BAR_H - 3, SCREEN_W, 3, color_accent);
    z_draw_text(22, 13, "ACTIVITIES", color_text, 2);
    z_draw_text(574, 13, "ZENITHOS", color_accent, 2);
    z_draw_text(1030, 13, state->status, color_muted, 2);
}

static void draw_overview_tile(uint64_t x, uint64_t y, const char *label, bool hover)
{
    z_draw_rect(x, y, 150, 88, hover ? color_accent : color_panel_hi);
    z_draw_rect(x + 18, y + 16, 34, 34, hover ? color_text : color_accent);
    z_draw_text(x + 18, y + 60, label, hover ? color_top : color_text, 2);
}

static void draw_overview(const struct session_state *state)
{
    z_draw_rect(0, TOP_BAR_H, SCREEN_W, SCREEN_H - TOP_BAR_H, 0x181c22);
    z_draw_text(96, 88, "OVERVIEW", color_text, 4);
    z_draw_text(98, 142, "APP LAUNCHER AND WINDOW PICKER", color_muted, 2);

    draw_overview_tile(120, 210, "TERMINAL",
                       point_in(state->cursor_x, state->cursor_y, 120, 210, 150, 88));
    draw_overview_tile(300, 210, "SETTINGS",
                       point_in(state->cursor_x, state->cursor_y, 300, 210, 150, 88));
    draw_overview_tile(480, 210, "FILES",
                       point_in(state->cursor_x, state->cursor_y, 480, 210, 150, 88));
    draw_overview_tile(660, 210, "PACKAGES",
                       point_in(state->cursor_x, state->cursor_y, 660, 210, 150, 88));

    z_draw_rect(360, 690, 560, 58, color_panel);
    z_draw_text(420, 710, "DASH  TERMINAL  SETTINGS  FILES  PKG", color_text, 2);
}

static void draw_session(const struct session_state *state)
{
    z_draw_rect(0, 0, SCREEN_W, SCREEN_H, color_wallpaper);
    draw_top_bar(state);

    if (state->mode == MODE_OVERVIEW) {
        draw_overview(state);
    } else {
        for (uint64_t i = 0; i < MAX_WINDOWS; i++) {
            const struct window *window = &state->windows[i];
            if (window->used &&
                window->role != Z_UI_ROLE_LAUNCHER &&
                window->visible &&
                !window->minimized) {
                draw_window(window, i == state->focus);
            }
        }
    }

    draw_cursor(state);
}

static void restore_role(struct session_state *state, uint64_t role)
{
    focus_role(state, role);
}

static uint64_t window_at_point(struct session_state *state, uint64_t x, uint64_t y)
{
    for (uint64_t step = 0; step < MAX_WINDOWS; step++) {
        uint64_t index = MAX_WINDOWS - 1 - step;
        struct window *window = &state->windows[index];
        if (window->used &&
            window->role != Z_UI_ROLE_LAUNCHER &&
            window->visible &&
            !window->minimized &&
            point_in(x, y, window->x, window->y, window->w, window->h)) {
            return index;
        }
    }

    return FOCUS_NONE;
}

static void handle_overview_click(struct session_state *state)
{
    uint64_t launcher = find_window_index_by_role(state, Z_UI_ROLE_LAUNCHER);
    if (launcher != FOCUS_NONE) {
        (void)z_ui_send(Z_UI_MSG_INPUT_MOUSE,
                        state->windows[launcher].pid,
                        state->cursor_x,
                        state->cursor_y,
                        Z_MOUSE_LEFT,
                        0);
        return;
    }

    if (point_in(state->cursor_x, state->cursor_y, 120, 210, 150, 88)) {
        restore_role(state, Z_UI_ROLE_TERMINAL);
    } else if (point_in(state->cursor_x, state->cursor_y, 300, 210, 150, 88)) {
        restore_role(state, Z_UI_ROLE_SETTINGS);
    }
}

static void handle_click(struct session_state *state)
{
    if (point_in(state->cursor_x, state->cursor_y, 0, 0, 170, TOP_BAR_H)) {
        state->mode = state->mode == MODE_OVERVIEW ? MODE_NORMAL : MODE_OVERVIEW;
        return;
    }

    if (state->mode == MODE_OVERVIEW) {
        handle_overview_click(state);
        return;
    }

    uint64_t index = window_at_point(state, state->cursor_x, state->cursor_y);
    if (index == FOCUS_NONE) {
        return;
    }

    struct window *window = &state->windows[index];
    state->focus = index;

    if (point_in(state->cursor_x, state->cursor_y,
                 window->x + window->w - 34, window->y + 6, 28, 28)) {
        window->visible = false;
        (void)z_ui_send(Z_UI_MSG_CLOSE, window->pid, 0, 0, 0, 0);
        focus_next_window(state);
        return;
    }

    if (point_in(state->cursor_x, state->cursor_y,
                 window->x + window->w - 64, window->y + 6, 28, 28)) {
        window->minimized = true;
        focus_next_window(state);
        return;
    }

    if (point_in(state->cursor_x, state->cursor_y, window->x, window->y, window->w, TITLE_H)) {
        state->drag = index;
        return;
    }

    send_mouse_to_window(window, state->cursor_x, state->cursor_y, Z_MOUSE_LEFT);
}

static void handle_mouse(struct session_state *state, uint64_t code, uint64_t raw_value)
{
    uint64_t buttons = code & UINT64_C(0xff);
    int64_t dx = sign_extend16(raw_value);
    int64_t dy = sign_extend16(raw_value >> 16);
    bool left_down = (buttons & Z_MOUSE_LEFT) != 0;
    bool left_click = left_down && (state->mouse_buttons & Z_MOUSE_LEFT) == 0;

    state->cursor_x = clamp_add(state->cursor_x, dx, 0, SCREEN_W - 24);
    state->cursor_y = clamp_add(state->cursor_y, dy, TOP_BAR_H, SCREEN_H - 28);
    state->mouse_buttons = buttons;

    if (!left_down) {
        state->drag = DRAG_NONE;
    } else if (state->drag != DRAG_NONE && state->drag < MAX_WINDOWS) {
        struct window *window = &state->windows[state->drag];
        window->x = clamp_add(window->x, dx, 0, SCREEN_W - 160);
        window->y = clamp_add(window->y, dy, TOP_BAR_H, SCREEN_H - 120);
    }

    if (left_click) {
        handle_click(state);
    }
}

static void handle_key(struct session_state *state, uint64_t key)
{
    if (key == '\t') {
        if (state->focus != FOCUS_NONE &&
            state->focus < MAX_WINDOWS &&
            state->windows[state->focus].role == Z_UI_ROLE_TERMINAL) {
            send_key_to_focused(state, key);
            return;
        }
        focus_next_window(state);
        return;
    }

    if (key == 27) {
        state->mode = MODE_NORMAL;
        return;
    }

    if (state->mode == MODE_OVERVIEW) {
        uint64_t launcher = find_window_index_by_role(state, Z_UI_ROLE_LAUNCHER);
        if (launcher != FOCUS_NONE) {
            (void)z_ui_send(Z_UI_MSG_INPUT_KEY, state->windows[launcher].pid, key, 0, 0, 0);
        }
        return;
    }

    send_key_to_focused(state, key);
}

static void handle_input(struct session_state *state)
{
    uint64_t event = z_event_poll();
    uint64_t type = (event >> 56) & UINT64_C(0xff);
    uint64_t code = (event >> 32) & UINT64_C(0xffffff);
    uint64_t value = event & UINT64_C(0xffffffff);

    if (type == Z_INPUT_EVENT_KEY && code != 0) {
        handle_key(state, code);
    } else if (type == Z_INPUT_EVENT_MOUSE) {
        handle_mouse(state, code, value);
    }
}

static void init_state(struct session_state *state)
{
    for (uint64_t i = 0; i < MAX_WINDOWS; i++) {
        state->windows[i].used = false;
    }

    state->focus = FOCUS_NONE;
    state->drag = DRAG_NONE;
    state->cursor_x = 180;
    state->cursor_y = 180;
    state->mouse_buttons = 0;
    state->mode = MODE_NORMAL;
    state->terminal_history_count = 0;
    state->terminal_history_next = 0;
    set_status(state, "SESSION READY");
}

static void spawn_apps(void)
{
    (void)z_spawn(terminal_path, 0);
    (void)z_spawn(settings_path, 0);
    (void)z_spawn(launcher_path, 0);
}

int main(const char *arg)
{
    (void)arg;

    struct session_state *state = (struct session_state *)(uintptr_t)USER_HEAP_BASE;

    z_map_page(USER_HEAP_BASE, 1);
    z_map_page(USER_HEAP_BASE + UINT64_C(0x1000), 1);
    z_ui_bind_session();
    init_state(state);
    spawn_apps();

    for (;;) {
        pump_ui_messages(state);
        handle_input(state);
        pump_ui_messages(state);
        draw_session(state);
        z_sleep_ticks(1);
    }
}
