#include "zenith/panic.h"

#include "zenith/framebuffer.h"
#include "zenith/halt.h"
#include "zenith/serial.h"

static struct limine_framebuffer *panic_framebuffer;

void panic_set_framebuffer(struct limine_framebuffer *framebuffer)
{
    panic_framebuffer = framebuffer;
}

ZENITH_NORETURN void panic(const char *message)
{
    __asm__ volatile ("cli");

    serial_write("[panic] ");
    serial_write(message);
    serial_write("\n");

    framebuffer_panic_screen(panic_framebuffer);
    zenith_halt();
}

