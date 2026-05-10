#include "zenith/framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static struct limine_framebuffer *active_framebuffer;

struct glyph {
    char character;
    uint8_t rows[7];
};

static const struct glyph glyphs[] = {
    { ' ', { 0, 0, 0, 0, 0, 0, 0 } },
    { '0', { 14, 17, 19, 21, 25, 17, 14 } },
    { '1', { 4, 12, 4, 4, 4, 4, 14 } },
    { '2', { 14, 17, 1, 2, 4, 8, 31 } },
    { '3', { 30, 1, 1, 14, 1, 1, 30 } },
    { '4', { 2, 6, 10, 18, 31, 2, 2 } },
    { '5', { 31, 16, 16, 30, 1, 1, 30 } },
    { '6', { 14, 16, 16, 30, 17, 17, 14 } },
    { '7', { 31, 1, 2, 4, 8, 8, 8 } },
    { '8', { 14, 17, 17, 14, 17, 17, 14 } },
    { '9', { 14, 17, 17, 15, 1, 1, 14 } },
    { 'A', { 14, 17, 17, 31, 17, 17, 17 } },
    { 'B', { 30, 17, 17, 30, 17, 17, 30 } },
    { 'C', { 14, 17, 16, 16, 16, 17, 14 } },
    { 'D', { 30, 17, 17, 17, 17, 17, 30 } },
    { 'E', { 31, 16, 16, 30, 16, 16, 31 } },
    { 'F', { 31, 16, 16, 30, 16, 16, 16 } },
    { 'G', { 14, 17, 16, 23, 17, 17, 14 } },
    { 'H', { 17, 17, 17, 31, 17, 17, 17 } },
    { 'I', { 14, 4, 4, 4, 4, 4, 14 } },
    { 'J', { 7, 2, 2, 2, 2, 18, 12 } },
    { 'K', { 17, 18, 20, 24, 20, 18, 17 } },
    { 'L', { 16, 16, 16, 16, 16, 16, 31 } },
    { 'M', { 17, 27, 21, 21, 17, 17, 17 } },
    { 'N', { 17, 25, 21, 19, 17, 17, 17 } },
    { 'O', { 14, 17, 17, 17, 17, 17, 14 } },
    { 'P', { 30, 17, 17, 30, 16, 16, 16 } },
    { 'Q', { 14, 17, 17, 17, 21, 18, 13 } },
    { 'R', { 30, 17, 17, 30, 20, 18, 17 } },
    { 'S', { 15, 16, 16, 14, 1, 1, 30 } },
    { 'T', { 31, 4, 4, 4, 4, 4, 4 } },
    { 'U', { 17, 17, 17, 17, 17, 17, 14 } },
    { 'V', { 17, 17, 17, 17, 17, 10, 4 } },
    { 'W', { 17, 17, 17, 21, 21, 21, 10 } },
    { 'X', { 17, 17, 10, 4, 10, 17, 17 } },
    { 'Y', { 17, 17, 10, 4, 4, 4, 4 } },
    { 'Z', { 31, 1, 2, 4, 8, 16, 31 } },
    { ':', { 0, 4, 4, 0, 4, 4, 0 } },
    { '.', { 0, 0, 0, 0, 0, 12, 12 } },
    { ',', { 0, 0, 0, 0, 0, 4, 8 } },
    { '-', { 0, 0, 0, 31, 0, 0, 0 } },
    { '/', { 1, 2, 2, 4, 8, 8, 16 } },
    { '>', { 16, 8, 4, 2, 4, 8, 16 } },
    { '_', { 0, 0, 0, 0, 0, 0, 31 } },
    { '?', { 14, 17, 1, 2, 4, 0, 4 } },
    { '!', { 4, 4, 4, 4, 4, 0, 4 } }
};

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void put_pixel(struct limine_framebuffer *fb, uint64_t x, uint64_t y, uint32_t color)
{
    if (x >= fb->width || y >= fb->height || fb->bpp != 32) {
        return;
    }

    uint8_t *row = (uint8_t *)fb->address + (y * fb->pitch);
    uint32_t *pixel = (uint32_t *)(row + (x * 4));
    *pixel = color;
}

static void fill_rect(struct limine_framebuffer *fb, uint64_t x, uint64_t y,
                      uint64_t width, uint64_t height, uint32_t color)
{
    for (uint64_t py = y; py < y + height && py < fb->height; py++) {
        for (uint64_t px = x; px < x + width && px < fb->width; px++) {
            put_pixel(fb, px, py, color);
        }
    }
}

static const uint8_t *find_glyph(char character)
{
    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
        if (glyphs[i].character == character) {
            return glyphs[i].rows;
        }
    }

    return glyphs[0].rows;
}

static void draw_char(struct limine_framebuffer *fb, uint64_t x, uint64_t y,
                      char character, uint32_t color, uint64_t scale)
{
    const uint8_t *rows = find_glyph(character);

    for (uint64_t row = 0; row < 7; row++) {
        for (uint64_t col = 0; col < 5; col++) {
            bool on = (rows[row] & (uint8_t)(1u << (4 - col))) != 0;
            if (on) {
                fill_rect(fb, x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void draw_text(struct limine_framebuffer *fb, uint64_t x, uint64_t y,
                      const char *text, uint32_t color, uint64_t scale)
{
    uint64_t cursor = x;

    while (*text != '\0') {
        draw_char(fb, cursor, y, *text++, color, scale);
        cursor += 6 * scale;
    }
}

void framebuffer_init(struct limine_framebuffer *framebuffer)
{
    active_framebuffer = framebuffer;
}

void framebuffer_fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t color)
{
    if (active_framebuffer == NULL) {
        return;
    }

    fill_rect(active_framebuffer, x, y, width, height, color);
}

void framebuffer_draw_text(uint64_t x, uint64_t y, const char *text, uint32_t color, uint64_t scale)
{
    if (active_framebuffer == NULL || text == NULL) {
        return;
    }

    if (scale == 0) {
        scale = 1;
    }

    draw_text(active_framebuffer, x, y, text, color, scale);
}

void framebuffer_boot_screen(struct limine_framebuffer *fb)
{
    if (fb == NULL || fb->address == NULL || fb->bpp != 32) {
        return;
    }

    framebuffer_init(fb);

    uint32_t top = rgb(18, 31, 48);
    uint32_t panel = rgb(36, 48, 64);
    uint32_t accent = rgb(74, 167, 138);
    uint32_t text = rgb(238, 241, 245);

    fill_rect(fb, 0, 0, fb->width, fb->height, top);

    uint64_t card_width = fb->width > 760 ? 720 : fb->width - 40;
    uint64_t card_height = 220;
    uint64_t card_x = (fb->width - card_width) / 2;
    uint64_t card_y = (fb->height - card_height) / 2;

    fill_rect(fb, card_x, card_y, card_width, card_height, panel);
    fill_rect(fb, card_x, card_y, card_width, 8, accent);

    draw_text(fb, card_x + 40, card_y + 52, "ZENITHOS", text, 6);
    draw_text(fb, card_x + 42, card_y + 126, "BOOT OK", accent, 4);
    draw_text(fb, card_x + 42, card_y + 168, "LIMINE GOP READY", text, 3);
}

void framebuffer_panic_screen(struct limine_framebuffer *fb)
{
    if (fb == NULL || fb->address == NULL || fb->bpp != 32) {
        return;
    }

    uint32_t background = rgb(86, 28, 36);
    uint32_t panel = rgb(122, 38, 49);
    uint32_t text = rgb(255, 245, 245);

    fill_rect(fb, 0, 0, fb->width, fb->height, background);

    uint64_t card_width = fb->width > 760 ? 720 : fb->width - 40;
    uint64_t card_height = 190;
    uint64_t card_x = (fb->width - card_width) / 2;
    uint64_t card_y = (fb->height - card_height) / 2;

    fill_rect(fb, card_x, card_y, card_width, card_height, panel);
    draw_text(fb, card_x + 40, card_y + 52, "KERNEL PANIC", text, 5);
    draw_text(fb, card_x + 42, card_y + 126, "SEE SERIAL LOG", text, 3);
}
