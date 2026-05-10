#include "zenith/serial.h"

#include "io.h"

enum {
    COM1 = 0x3f8
};

static int serial_can_write(void)
{
    return (inb(COM1 + 5) & 0x20) != 0;
}

static int serial_can_read(void)
{
    return (inb(COM1 + 5) & 0x01) != 0;
}

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xc7);
    outb(COM1 + 4, 0x0b);
}

void serial_write_char(char value)
{
    while (!serial_can_write()) {
    }

    outb(COM1, (uint8_t)value);
}

bool serial_try_read_char(char *value_out)
{
    if (value_out == 0 || !serial_can_read()) {
        return false;
    }

    *value_out = (char)inb(COM1);
    return true;
}

void serial_write(const char *message)
{
    while (*message != '\0') {
        if (*message == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(*message++);
    }
}

void serial_write_u64_hex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";

    serial_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_write_char(digits[(value >> shift) & 0xf]);
    }
}

void serial_write_u64_dec(uint64_t value)
{
    char buffer[21];
    unsigned int index = 0;

    if (value == 0) {
        serial_write_char('0');
        return;
    }

    while (value != 0 && index < sizeof(buffer)) {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        serial_write_char(buffer[--index]);
    }
}
