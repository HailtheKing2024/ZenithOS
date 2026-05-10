#include "zenith/user.h"

#include <stdbool.h>

enum {
    TERM_LINES = 10,
    TERM_CAP = Z_UI_TEXT_CAP
};

static char lines[TERM_LINES][TERM_CAP];
static char input[TERM_CAP];
static uint64_t line_count;
static uint64_t input_len;

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
    uint64_t index = 0;

    if (dest_size == 0) {
        return;
    }

    while (index + 1 < dest_size && src[index] != '\0') {
        dest[index] = src[index];
        index++;
    }

    dest[index] = '\0';
}

static void compose_prompt(char *dest, uint64_t dest_size)
{
    const char prefix[] = "ZENITH> ";
    uint64_t index = 0;

    while (index + 1 < dest_size && prefix[index] != '\0') {
        dest[index] = prefix[index];
        index++;
    }

    for (uint64_t i = 0; index + 1 < dest_size && input[i] != '\0'; i++) {
        dest[index++] = input[i];
    }

    dest[index] = '\0';
}

static void push_line(const char *text)
{
    if (line_count < TERM_LINES - 1) {
        copy_text(lines[line_count], TERM_CAP, text);
        line_count++;
        return;
    }

    for (uint64_t row = 1; row < TERM_LINES - 1; row++) {
        copy_text(lines[row - 1], TERM_CAP, lines[row]);
    }

    copy_text(lines[TERM_LINES - 2], TERM_CAP, text);
}

static void redraw(void)
{
    char prompt[TERM_CAP];

    (void)z_ui_send(Z_UI_MSG_CLEAR, 0, 0, 0, 0, 0);
    for (uint64_t row = 0; row < line_count; row++) {
        (void)z_ui_send(Z_UI_MSG_SET_LINE, 0, row, 0, 0, lines[row]);
    }

    compose_prompt(prompt, sizeof(prompt));
    (void)z_ui_send(Z_UI_MSG_SET_LINE, 0, TERM_LINES - 1, 0, 0, prompt);
}

static void clear_terminal(void)
{
    line_count = 0;
    input_len = 0;
    input[0] = '\0';
    for (uint64_t row = 0; row < TERM_LINES; row++) {
        lines[row][0] = '\0';
    }
}

static void run_command(void)
{
    if (input[0] == '\0') {
        return;
    }

    if (string_equals(input, "HELP")) {
        push_line("COMMANDS HELP SYSINFO ECHO CLEAR");
        push_line("SETTINGS OVERVIEW DESKTOP REBOOT");
        push_line("TAB CHANGES FOCUS MOUSE CLICKS WINDOWS");
    } else if (string_equals(input, "SYSINFO")) {
        push_line("ZENITHOS SESSION ARCHITECTURE");
        push_line("WINDOW SERVER AND APP IPC ONLINE");
        push_line("APPS TERMINAL SETTINGS LAUNCHER");
    } else if (string_starts_with(input, "ECHO ")) {
        push_line(input + 5);
    } else if (string_equals(input, "ECHO")) {
        push_line("");
    } else if (string_equals(input, "CLEAR")) {
        clear_terminal();
        return;
    } else if (string_equals(input, "SETTINGS")) {
        (void)z_ui_send(Z_UI_MSG_COMMAND, 0, Z_UI_CMD_SHOW_SETTINGS, 0, 0, "SETTINGS");
        push_line("SETTINGS WINDOW REQUESTED");
    } else if (string_equals(input, "OVERVIEW")) {
        (void)z_ui_send(Z_UI_MSG_COMMAND, 0, Z_UI_CMD_SHOW_OVERVIEW, 0, 0, "OVERVIEW");
        push_line("OVERVIEW REQUESTED");
    } else if (string_equals(input, "DESKTOP")) {
        push_line("SESSION OWNS TOP BAR WINDOWS CURSOR");
        push_line("APPS SEND DRAW REQUESTS OVER UI IPC");
    } else if (string_equals(input, "REBOOT")) {
        push_line("REBOOT IS NOT WIRED YET");
    } else {
        push_line("UNKNOWN COMMAND");
    }
}

static void handle_key(uint64_t key)
{
    char value = to_upper((char)(uint8_t)key);

    if (value == '\n' || value == '\r') {
        run_command();
        input_len = 0;
        input[0] = '\0';
        redraw();
        return;
    }

    if (value == '\b' || value == 127) {
        if (input_len > 0) {
            input_len--;
            input[input_len] = '\0';
        }
        redraw();
        return;
    }

    if (value < ' ' || value > '~' || input_len + 1 >= TERM_CAP) {
        return;
    }

    input[input_len++] = value;
    input[input_len] = '\0';
    redraw();
}

int main(const char *arg)
{
    (void)arg;

    clear_terminal();
    push_line("TERMINAL APP READY");
    push_line("TYPE HELP THEN ENTER");

    (void)z_ui_send(Z_UI_MSG_REGISTER, 0, Z_UI_ROLE_TERMINAL, 760, 472, "TERMINAL");
    redraw();

    for (;;) {
        struct z_ui_message message;
        if (z_ui_recv(&message) == 0) {
            if (message.type == Z_UI_MSG_INPUT_KEY) {
                handle_key(message.a);
            } else if (message.type == Z_UI_MSG_INPUT_MOUSE) {
                (void)z_ui_send(Z_UI_MSG_NOTIFY, 0, Z_UI_CMD_NOTIFY, 0, 0, "TERMINAL FOCUSED");
            } else if (message.type == Z_UI_MSG_CLOSE) {
                z_exit(0);
            }
        } else {
            z_sleep_ticks(1);
        }
    }
}
