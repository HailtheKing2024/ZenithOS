#include "zenith/console.h"

#include "zenith/framebuffer.h"
#include "zenith/serial.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    CONSOLE_SCREEN_WIDTH = 1280,
    CONSOLE_SCREEN_HEIGHT = 800,
    CONSOLE_TOP_HEIGHT = 54,
    CONSOLE_TEXT_X = 24,
    CONSOLE_TEXT_Y = 76,
    CONSOLE_SCALE = 2,
    CONSOLE_ROW_HEIGHT = 20,
    CONSOLE_ROWS = 34,
    CONSOLE_LINE_CAP = 72
};

static const char prompt[] = "ZENITH> ";
static const uint32_t color_background = 0x121820;
static const uint32_t color_header = 0x243240;
static const uint32_t color_accent = 0x4aa78a;
static const uint32_t color_text = 0xf2f4f6;
static const uint32_t color_muted = 0x9fb0bd;

static bool initialized;
static uint64_t row;
static char line[CONSOLE_LINE_CAP];
static char completed_line[CONSOLE_LINE_CAP];
static uint64_t line_length;
static bool completed_ready;

static bool char_is_lower(char value)
{
    return value >= 'a' && value <= 'z';
}

static char char_to_upper(char value)
{
    if (char_is_lower(value)) {
        return (char)(value - 'a' + 'A');
    }

    return value;
}

static uint64_t copy_string(char *dest, size_t dest_size, const char *src)
{
    if (dest_size == 0) {
        return 0;
    }

    size_t index = 0;
    while (index + 1 < dest_size && src[index] != '\0') {
        dest[index] = src[index];
        index++;
    }

    dest[index] = '\0';
    return index;
}

static void clear_row(uint64_t target_row)
{
    uint64_t y = CONSOLE_TEXT_Y + target_row * CONSOLE_ROW_HEIGHT;
    framebuffer_fill_rect(0, y, CONSOLE_SCREEN_WIDTH, CONSOLE_ROW_HEIGHT, color_background);
}

static void draw_row(uint64_t target_row, const char *text, uint32_t color)
{
    clear_row(target_row);
    framebuffer_draw_text(CONSOLE_TEXT_X, CONSOLE_TEXT_Y + target_row * CONSOLE_ROW_HEIGHT,
                          text, color, CONSOLE_SCALE);
}

static void clear_body(void)
{
    framebuffer_fill_rect(0, CONSOLE_TOP_HEIGHT, CONSOLE_SCREEN_WIDTH,
                          CONSOLE_SCREEN_HEIGHT - CONSOLE_TOP_HEIGHT, color_background);
}

static void advance_row(void)
{
    row++;
    if (row >= CONSOLE_ROWS) {
        clear_body();
        row = 0;
    }
}

static void print_line(const char *text, uint32_t color)
{
    draw_row(row, text, color);
    advance_row();
}

static void render_prompt(void)
{
    char buffer[sizeof(prompt) + CONSOLE_LINE_CAP];
    size_t index = 0;

    while (prompt[index] != '\0') {
        buffer[index] = prompt[index];
        index++;
    }

    for (uint64_t i = 0; i < line_length && index + 1 < sizeof(buffer); i++) {
        buffer[index++] = line[i];
    }

    buffer[index] = '\0';
    draw_row(row, buffer, color_text);
}

static void reset_line(void)
{
    line_length = 0;
    line[0] = '\0';
}

static void complete_current_line(void)
{
    (void)copy_string(completed_line, sizeof(completed_line), line);
    completed_ready = true;
}

static void print_logo(void)
{
    print_line("ZZZZZ  EEEEE  N   N  III  TTTTT  H   H", color_accent);
    print_line("   Z   E      NN  N   I     T    H   H", color_accent);
    print_line("  Z    EEEE   N N N   I     T    HHHHH", color_accent);
    print_line(" Z     E      N  NN   I     T    H   H", color_accent);
    print_line("ZZZZZ  EEEEE  N   N  III    T    H   H", color_accent);
    print_line("", color_text);
}

void console_init(void)
{
    initialized = true;
    row = 0;
    reset_line();
    completed_line[0] = '\0';
    completed_ready = false;

    framebuffer_fill_rect(0, 0, CONSOLE_SCREEN_WIDTH, CONSOLE_SCREEN_HEIGHT, color_background);
    framebuffer_fill_rect(0, 0, CONSOLE_SCREEN_WIDTH, CONSOLE_TOP_HEIGHT, color_header);
    framebuffer_fill_rect(0, CONSOLE_TOP_HEIGHT - 4, CONSOLE_SCREEN_WIDTH, 4, color_accent);
    framebuffer_draw_text(24, 16, "ZENITHOS", color_text, 3);
    framebuffer_draw_text(190, 18, "WORKSTATION CONSOLE", color_accent, 2);

    print_logo();
    print_line("ZENITHOS CONSOLE READY", color_accent);
    print_line("LAUNCHING /BIN/SH", color_muted);
    render_prompt();

    serial_write("[console] framebuffer console initialized\n");
}

uint64_t console_handle_key(uint64_t code)
{
    if (!initialized) {
        console_init();
    }

    if (code == 0) {
        return CONSOLE_ACTION_NONE;
    }

    char value = char_to_upper((char)(uint8_t)code);
    if (value == '\n' || value == '\r') {
        render_prompt();
        advance_row();
        complete_current_line();
        serial_write("[console] line ");
        serial_write(completed_line);
        serial_write("\n");
        reset_line();
        return CONSOLE_ACTION_LINE_READY;
    }

    if (value == '\b' || value == 127) {
        if (line_length > 0) {
            line_length--;
            line[line_length] = '\0';
            render_prompt();
        }
        return CONSOLE_ACTION_NONE;
    }

    if (value < ' ' || value > '~') {
        return CONSOLE_ACTION_NONE;
    }

    if (line_length + 1 >= CONSOLE_LINE_CAP) {
        return CONSOLE_ACTION_NONE;
    }

    line[line_length++] = value;
    line[line_length] = '\0';
    render_prompt();
    return CONSOLE_ACTION_NONE;
}

void console_write_line(const char *text)
{
    if (!initialized) {
        console_init();
    }

    print_line(text == NULL ? "" : text, color_accent);
}

void console_render_prompt(void)
{
    if (!initialized) {
        console_init();
    }

    render_prompt();
}

uint64_t console_read_line(char *dest, uint64_t dest_size)
{
    if (dest == NULL || dest_size == 0) {
        return UINT64_MAX;
    }

    if (!completed_ready) {
        return UINT64_MAX;
    }

    uint64_t copied = copy_string(dest, (size_t)dest_size, completed_line);
    completed_ready = false;
    completed_line[0] = '\0';
    return copied;
}

void console_clear(void)
{
    if (!initialized) {
        console_init();
    }

    clear_body();
    row = 0;
    reset_line();
}

uint64_t console_copy_argument(char *dest, uint64_t dest_size)
{
    if (dest == NULL || dest_size == 0) {
        return 0;
    }

    dest[0] = '\0';
    return 0;
}
