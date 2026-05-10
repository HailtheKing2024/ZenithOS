#ifndef ZENITH_FRAMEBUFFER_H
#define ZENITH_FRAMEBUFFER_H

#include "zenith/limine.h"

#include <stdint.h>

void framebuffer_init(struct limine_framebuffer *framebuffer);
void framebuffer_fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t color);
void framebuffer_draw_text(uint64_t x, uint64_t y, const char *text, uint32_t color, uint64_t scale);
void framebuffer_boot_screen(struct limine_framebuffer *framebuffer);
void framebuffer_panic_screen(struct limine_framebuffer *framebuffer);

#endif
