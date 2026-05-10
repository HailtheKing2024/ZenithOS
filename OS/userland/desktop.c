#include "zenith/user.h"

#include <stdbool.h>

enum {
    SCREEN_W = 1280,
    SCREEN_H = 800,
    TOP_BAR_H = 44,

    TERM_X = 72,
    TERM_Y = 96,
    TERM_W = 760,
    TERM_H = 548,

    OVERVIEW_X = 864,
    OVERVIEW_Y = 96,
    OVERVIEW_W = 344,
    OVERVIEW_H = 300,

    SETTINGS_X = 864,
    SETTINGS_Y = 430,
    SETTINGS_W = 344,
    SETTINGS_H = 230,

    TERM_ROWS = 10,
    LINE_CAP = 64,

    FOCUS_TERMINAL = 0,
    FOCUS_OVERVIEW = 1,
    FOCUS_SETTINGS = 2
};

struct desktop_state {
    char line[LINE_CAP];
    char rows[TERM_ROWS][LINE_CAP];
    uint64_t line_len;
    uint64_t row_count;
    uint64_t focus;
    uint64_t cursor_x;
    uint64_t cursor_y;
    uint64_t mouse_buttons;
};

static const uint32_t color_bg = 0x20252b;
static const uint32_t color_top = 0x11161b;
static const uint32_t color_panel = 0x2b323a;
static const uint32_t color_panel_2 = 0x353d45;
static const uint32_t color_text = 0xf4f6f7;
static const uint32_t color_muted = 0xaeb9c2;
static const uint32_t color_accent = 0x4aa78a;
static const uint32_t color_focus = 0x6da7ff;
static const uint32_t color_warn = 0xe0b35a;
static const uint32_t color_shadow = 0x12161a;

static char to_upper(char value)
{
    if (value >= 'a' && value <= 'z') {
        return (char)(value - 'a' + 'A');
    }

    return value;
}

static bool string_equals(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (*left++ != *right++) {
            return false;
        }
    }

    return *left == '\0' && *right == '\0';
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

static void copy_text(char *dest, uint64_t dest_size, const char *src)
{
    if (dest_size == 0) {
        return;
    }

    uint64_t index = 0;
    while (index + 1 < dest_size && src[index] != '\0') {
        dest[index] = src[index];
        index++;
    }

    dest[index] = '\0';
}

static void compose_prompt(char *dest, uint64_t dest_size, const char *line)
{
    const char prefix[] = "ZENITH> ";
    uint64_t index = 0;

    while (index + 1 < dest_size && prefix[index] != '\0') {
        dest[index] = prefix[index];
        index++;
    }

    for (uint64_t i = 0; index + 1 < dest_size && line[i] != '\0'; i++) {
        dest[index++] = line[i];
    }

    dest[index] = '\0';
}

static void push_output(struct desktop_state *state, const char *text)
{
    if (state->row_count < TERM_ROWS) {
        copy_text(state->rows[state->row_count], LINE_CAP, text);
        state->row_count++;
        return;
    }

    for (uint64_t row = 1; row < TERM_ROWS; row++) {
        copy_text(state->rows[row - 1], LINE_CAP, state->rows[row]);
    }

    copy_text(state->rows[TERM_ROWS - 1], LINE_CAP, text);
}

static void clear_terminal(struct desktop_state *state)
{
    state->row_count = 0;
    for (uint64_t row = 0; row < TERM_ROWS; row++) {
        state->rows[row][0] = '\0';
    }
}

static bool point_in(uint64_t px, uint64_t py, uint64_t x, uint64_t y, uint64_t w, uint64_t h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void draw_window(uint64_t x,
                        uint64_t y,
                        uint64_t w,
                        uint64_t h,
                        const char *title,
                        bool focused)
{
    z_draw_rect(x + 8, y + 8, w, h, color_shadow);
    z_draw_rect(x, y, w, h, focused ? color_focus : color_panel_2);
    z_draw_rect(x + 2, y + 2, w - 4, h - 4, color_panel);
    z_draw_rect(x + 2, y + 2, w - 4, 38, focused ? color_focus : color_panel_2);
    z_draw_text(x + 18, y + 13, title, color_text, 2);
}

static void draw_tile(uint64_t x, uint64_t y, const char *title, bool hovered)
{
    z_draw_rect(x, y, 138, 72, hovered ? color_accent : color_panel_2);
    z_draw_rect(x + 10, y + 10, 32, 32, hovered ? color_text : color_accent);
    z_draw_text(x + 54, y + 18, title, hovered ? color_top : color_text, 2);
}

static void draw_cursor(const struct desktop_state *state)
{
    uint32_t cursor_color = (state->mouse_buttons & Z_MOUSE_LEFT) != 0 ? color_accent : color_text;

    z_draw_rect(state->cursor_x, state->cursor_y, 4, 24, cursor_color);
    z_draw_rect(state->cursor_x + 4, state->cursor_y + 4, 4, 16, cursor_color);
    z_draw_rect(state->cursor_x + 8, state->cursor_y + 8, 4, 12, cursor_color);
    z_draw_rect(state->cursor_x + 12, state->cursor_y + 20, 12, 4, cursor_color);
}

static void draw_terminal(const struct desktop_state *state)
{
    char prompt[LINE_CAP + 10];

    draw_window(TERM_X, TERM_Y, TERM_W, TERM_H, "TERMINAL", state->focus == FOCUS_TERMINAL);
    z_draw_rect(TERM_X + 18, TERM_Y + 54, TERM_W - 36, TERM_H - 108, 0x151a1f);

    for (uint64_t row = 0; row < state->row_count; row++) {
        z_draw_text(TERM_X + 36, TERM_Y + 76 + row * 28, state->rows[row], color_text, 2);
    }

    z_draw_rect(TERM_X + 18, TERM_Y + TERM_H - 44, TERM_W - 36, 26, 0x101418);
    compose_prompt(prompt, sizeof(prompt), state->line);
    z_draw_text(TERM_X + 36, TERM_Y + TERM_H - 38, prompt, color_accent, 2);
}

static void draw_overview(const struct desktop_state *state)
{
    draw_window(OVERVIEW_X, OVERVIEW_Y, OVERVIEW_W, OVERVIEW_H,
                "ACTIVITY OVERVIEW", state->focus == FOCUS_OVERVIEW);

    uint64_t x0 = OVERVIEW_X + 24;
    uint64_t y0 = OVERVIEW_Y + 64;
    draw_tile(x0, y0, "TERM", point_in(state->cursor_x, state->cursor_y, x0, y0, 138, 72));
    draw_tile(x0 + 158, y0, "SETTINGS",
              point_in(state->cursor_x, state->cursor_y, x0 + 158, y0, 138, 72));
    draw_tile(x0, y0 + 96, "FILES", point_in(state->cursor_x, state->cursor_y, x0, y0 + 96, 138, 72));
    draw_tile(x0 + 158, y0 + 96, "PKG",
              point_in(state->cursor_x, state->cursor_y, x0 + 158, y0 + 96, 138, 72));

    z_draw_text(OVERVIEW_X + 24, OVERVIEW_Y + 258, "APP GRID PROTOTYPE", color_muted, 2);
}

static void draw_settings(const struct desktop_state *state)
{
    draw_window(SETTINGS_X, SETTINGS_Y, SETTINGS_W, SETTINGS_H,
                "SYSTEM SETTINGS", state->focus == FOCUS_SETTINGS);
    z_draw_text(SETTINGS_X + 24, SETTINGS_Y + 68, "NETWORK    OFFLINE", color_text, 2);
    z_draw_text(SETTINGS_X + 24, SETTINGS_Y + 104, "AUDIO      PIPEWIRE TARGET", color_text, 2);
    z_draw_text(SETTINGS_X + 24, SETTINGS_Y + 140, "THEME      ADWAITA TOKENS", color_text, 2);
    z_draw_text(SETTINGS_X + 24, SETTINGS_Y + 176, "PACKAGES   APT FLATPAK PLAN", color_warn, 2);
    z_draw_rect(SETTINGS_X + 226, SETTINGS_Y + 58, 80, 28, color_panel_2);
    z_draw_text(SETTINGS_X + 238, SETTINGS_Y + 66, "INFO", color_text, 2);
    z_draw_rect(SETTINGS_X + 226, SETTINGS_Y + 166, 80, 28, color_panel_2);
    z_draw_text(SETTINGS_X + 238, SETTINGS_Y + 174, "PLAN", color_text, 2);
}

static void draw_desktop(const struct desktop_state *state)
{
    z_draw_rect(0, 0, SCREEN_W, SCREEN_H, color_bg);
    z_draw_rect(0, 0, SCREEN_W, TOP_BAR_H, color_top);
    z_draw_rect(0, TOP_BAR_H - 3, SCREEN_W, 3, color_accent);

    z_draw_text(22, 14, "ACTIVITIES", color_text, 2);
    z_draw_text(560, 14, "ZENITHOS", color_accent, 2);
    z_draw_text(1048, 14, "SESSION READY", color_muted, 2);

    draw_terminal(state);
    draw_overview(state);
    draw_settings(state);
    draw_cursor(state);
}

static void focus_next(struct desktop_state *state)
{
    state->focus = (state->focus + 1) % 3;
}

static void run_command(struct desktop_state *state)
{
    if (state->line[0] == '\0') {
        return;
    }

    if (string_equals(state->line, "HELP")) {
        push_output(state, "COMMANDS HELP SYSINFO ECHO CLEAR REBOOT");
        push_output(state, "DESKTOP SETTINGS OVERVIEW EXIT");
        push_output(state, "TAB FOCUS OVERVIEW USE WASD ENTER");
    } else if (string_equals(state->line, "SYSINFO")) {
        push_output(state, "ZENITHOS DESKTOP SESSION");
        push_output(state, "KERNEL USERLAND SYSCALLS ONLINE");
        push_output(state, "FRAMEBUFFER 1280X800 GOP");
    } else if (string_starts_with(state->line, "ECHO ")) {
        push_output(state, state->line + 5);
    } else if (string_equals(state->line, "ECHO")) {
        push_output(state, "");
    } else if (string_equals(state->line, "CLEAR")) {
        clear_terminal(state);
    } else if (string_equals(state->line, "REBOOT")) {
        push_output(state, "REBOOT IS NOT WIRED YET");
    } else if (string_equals(state->line, "DESKTOP")) {
        push_output(state, "ZENITHSHELL COMPOSITOR PROTOTYPE ACTIVE");
    } else if (string_equals(state->line, "SETTINGS")) {
        state->focus = FOCUS_SETTINGS;
    } else if (string_equals(state->line, "OVERVIEW")) {
        state->focus = FOCUS_OVERVIEW;
    } else if (string_equals(state->line, "EXIT")) {
        z_exit(0);
    } else {
        push_output(state, "UNKNOWN COMMAND");
    }
}

static void handle_terminal_key(struct desktop_state *state, char value)
{
    if (value == '\n' || value == '\r') {
        run_command(state);
        state->line_len = 0;
        state->line[0] = '\0';
        return;
    }

    if (value == '\b' || value == 127) {
        if (state->line_len > 0) {
            state->line_len--;
            state->line[state->line_len] = '\0';
        }
        return;
    }

    if (value < ' ' || value > '~' || state->line_len + 1 >= LINE_CAP) {
        return;
    }

    state->line[state->line_len++] = value;
    state->line[state->line_len] = '\0';
}

static void activate_overview(struct desktop_state *state)
{
    uint64_t x0 = OVERVIEW_X + 24;
    uint64_t y0 = OVERVIEW_Y + 64;

    if (point_in(state->cursor_x, state->cursor_y, x0, y0, 138, 72)) {
        state->focus = FOCUS_TERMINAL;
    } else if (point_in(state->cursor_x, state->cursor_y, x0 + 158, y0, 138, 72)) {
        state->focus = FOCUS_SETTINGS;
    } else if (point_in(state->cursor_x, state->cursor_y, x0, y0 + 96, 138, 72)) {
        state->focus = FOCUS_TERMINAL;
        push_output(state, "FILES APP IS NOT BUILT YET");
    } else if (point_in(state->cursor_x, state->cursor_y, x0 + 158, y0 + 96, 138, 72)) {
        state->focus = FOCUS_TERMINAL;
        push_output(state, "APT AND FLATPAK UI IS PLANNED");
    }
}

static void activate_click(struct desktop_state *state)
{
    if (point_in(state->cursor_x, state->cursor_y, 0, 0, 152, TOP_BAR_H)) {
        state->focus = FOCUS_OVERVIEW;
        return;
    }

    if (point_in(state->cursor_x, state->cursor_y, TERM_X, TERM_Y, TERM_W, TERM_H)) {
        state->focus = FOCUS_TERMINAL;
        return;
    }

    if (point_in(state->cursor_x, state->cursor_y, OVERVIEW_X, OVERVIEW_Y, OVERVIEW_W, OVERVIEW_H)) {
        state->focus = FOCUS_OVERVIEW;
        activate_overview(state);
        return;
    }

    if (point_in(state->cursor_x, state->cursor_y, SETTINGS_X, SETTINGS_Y, SETTINGS_W, SETTINGS_H)) {
        state->focus = FOCUS_SETTINGS;
        if (point_in(state->cursor_x, state->cursor_y, SETTINGS_X + 226, SETTINGS_Y + 58, 80, 28)) {
            state->focus = FOCUS_TERMINAL;
            push_output(state, "NETWORK SETTINGS ARE UI ONLY");
        } else if (point_in(state->cursor_x, state->cursor_y, SETTINGS_X + 226, SETTINGS_Y + 166, 80, 28)) {
            state->focus = FOCUS_TERMINAL;
            push_output(state, "APT AND FLATPAK PLAN IS NEXT");
        }
    }
}

static void move_cursor(struct desktop_state *state, char value)
{
    if (value == 'A' && state->cursor_x >= 24) {
        state->cursor_x -= 24;
    } else if (value == 'D' && state->cursor_x + 48 < SCREEN_W) {
        state->cursor_x += 24;
    } else if (value == 'W' && state->cursor_y >= TOP_BAR_H + 24) {
        state->cursor_y -= 24;
    } else if (value == 'S' && state->cursor_y + 48 < SCREEN_H) {
        state->cursor_y += 24;
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

static void handle_mouse(struct desktop_state *state, uint64_t code, uint64_t raw_value)
{
    uint64_t buttons = code & UINT64_C(0xff);
    int64_t dx = sign_extend16(raw_value);
    int64_t dy = sign_extend16(raw_value >> 16);
    bool left_click = (buttons & Z_MOUSE_LEFT) != 0 && (state->mouse_buttons & Z_MOUSE_LEFT) == 0;

    state->cursor_x = clamp_add(state->cursor_x, dx, 0, SCREEN_W - 24);
    state->cursor_y = clamp_add(state->cursor_y, dy, TOP_BAR_H, SCREEN_H - 28);
    state->mouse_buttons = buttons;

    if (left_click) {
        activate_click(state);
    }
}

static void handle_key(struct desktop_state *state, char value)
{
    value = to_upper(value);

    if (value == '\t') {
        focus_next(state);
        return;
    }

    if (state->focus == FOCUS_TERMINAL) {
        handle_terminal_key(state, value);
        return;
    }

    if (value == '\n' || value == '\r') {
        if (state->focus == FOCUS_OVERVIEW) {
            activate_overview(state);
        } else {
            state->focus = FOCUS_TERMINAL;
        }
        return;
    }

    move_cursor(state, value);
}

static void init_state(struct desktop_state *state)
{
    state->line_len = 0;
    state->line[0] = '\0';
    state->row_count = 0;
    state->focus = FOCUS_TERMINAL;
    state->cursor_x = OVERVIEW_X + 42;
    state->cursor_y = OVERVIEW_Y + 86;
    state->mouse_buttons = 0;
    clear_terminal(state);
    push_output(state, "ZENITHSHELL SESSION READY");
    push_output(state, "CLICK TERMINAL THEN TYPE HELP");
}

int main(const char *arg)
{
    (void)arg;

    struct desktop_state state;
    init_state(&state);
    draw_desktop(&state);

    for (;;) {
        uint64_t event = z_event_poll();
        uint64_t type = (event >> 56) & 0xff;
        uint64_t code = (event >> 32) & 0xffffff;
        uint64_t value = event & 0xffffffff;
        if (type == Z_INPUT_EVENT_KEY && code != 0) {
            handle_key(&state, (char)(uint8_t)code);
            draw_desktop(&state);
        } else if (type == Z_INPUT_EVENT_MOUSE) {
            handle_mouse(&state, code, value);
            draw_desktop(&state);
        }

        z_sleep_ticks(1);
    }
}
