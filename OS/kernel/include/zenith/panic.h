#ifndef ZENITH_PANIC_H
#define ZENITH_PANIC_H

#include "zenith/compiler.h"
#include "zenith/limine.h"

void panic_set_framebuffer(struct limine_framebuffer *framebuffer);
ZENITH_NORETURN void panic(const char *message);

#endif

