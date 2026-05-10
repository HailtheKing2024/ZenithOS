#include "zenith/keyboard.h"

#include "zenith/input.h"
#include "zenith/serial.h"

#include "../arch/x86_64/io.h"

enum {
    KEYBOARD_DATA_PORT = 0x60,
    PIC1_COMMAND = 0x20,
    PIC1_DATA = 0x21,
    PIC_EOI = 0x20
};

static char scancode_to_ascii(uint8_t scancode)
{
    static const char map[128] = {
        [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
        [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
        [0x0a] = '9', [0x0b] = '0', [0x0c] = '-', [0x0d] = '=',
        [0x0e] = '\b', [0x0f] = '\t',
        [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
        [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
        [0x18] = 'O', [0x19] = 'P',
        [0x1c] = '\n',
        [0x1e] = 'A', [0x1f] = 'S', [0x20] = 'D', [0x21] = 'F',
        [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
        [0x26] = 'L', [0x27] = ';', [0x28] = '\'',
        [0x2c] = 'Z', [0x2d] = 'X', [0x2e] = 'C', [0x2f] = 'V',
        [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = ',',
        [0x34] = '.', [0x35] = '/',
        [0x39] = ' '
    };

    if (scancode >= sizeof(map)) {
        return 0;
    }

    return map[scancode];
}

void keyboard_init(void)
{
    uint8_t mask = inb(PIC1_DATA);
    outb(PIC1_DATA, (uint8_t)(mask & ~(uint8_t)0x02));
    serial_write("[kbd] ps2 keyboard irq enabled\n");
}

struct interrupt_frame *keyboard_handle_irq(struct interrupt_frame *frame)
{
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    if ((scancode & 0x80) == 0) {
        char ascii = scancode_to_ascii(scancode);
        if (ascii != 0) {
            input_push_key((uint64_t)(uint8_t)ascii, 1);
        }
    }

    outb(PIC1_COMMAND, PIC_EOI);
    return frame;
}
