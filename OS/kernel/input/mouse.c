#include "zenith/mouse.h"

#include "zenith/input.h"
#include "zenith/serial.h"

#include "../arch/x86_64/io.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    PS2_DATA_PORT = 0x60,
    PS2_STATUS_PORT = 0x64,
    PS2_COMMAND_PORT = 0x64,
    PS2_STATUS_OUTPUT_FULL = 0x01,
    PS2_STATUS_INPUT_FULL = 0x02,
    PS2_CMD_READ_CONFIG = 0x20,
    PS2_CMD_WRITE_CONFIG = 0x60,
    PS2_CMD_ENABLE_AUX = 0xa8,
    PS2_CMD_WRITE_AUX = 0xd4,
    PS2_MOUSE_SET_DEFAULTS = 0xf6,
    PS2_MOUSE_ENABLE_STREAMING = 0xf4,
    PS2_ACK = 0xfa,
    PIC1_COMMAND = 0x20,
    PIC1_DATA = 0x21,
    PIC2_COMMAND = 0xa0,
    PIC2_DATA = 0xa1,
    PIC_EOI = 0x20,
    WAIT_LIMIT = 100000
};

static uint8_t packet[3];
static uint8_t packet_index;

static bool wait_input_clear(void)
{
    for (uint64_t i = 0; i < WAIT_LIMIT; i++) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) == 0) {
            return true;
        }
    }

    return false;
}

static bool wait_output_full(void)
{
    for (uint64_t i = 0; i < WAIT_LIMIT; i++) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0) {
            return true;
        }
    }

    return false;
}

static void controller_write(uint8_t value)
{
    if (wait_input_clear()) {
        outb(PS2_COMMAND_PORT, value);
    }
}

static void data_write(uint8_t value)
{
    if (wait_input_clear()) {
        outb(PS2_DATA_PORT, value);
    }
}

static uint8_t data_read(void)
{
    if (!wait_output_full()) {
        return 0;
    }

    return inb(PS2_DATA_PORT);
}

static void mouse_write(uint8_t value)
{
    controller_write(PS2_CMD_WRITE_AUX);
    data_write(value);
    (void)data_read();
}

static int16_t sign_extend_byte(uint8_t value, bool negative)
{
    int16_t result = (int16_t)value;
    if (negative) {
        result |= (int16_t)0xff00;
    }

    return result;
}

static void unmask_irq12(void)
{
    uint8_t master = inb(PIC1_DATA);
    uint8_t slave = inb(PIC2_DATA);

    outb(PIC1_DATA, (uint8_t)(master & ~(uint8_t)0x04));
    outb(PIC2_DATA, (uint8_t)(slave & ~(uint8_t)0x10));
}

void mouse_init(void)
{
    controller_write(PS2_CMD_ENABLE_AUX);

    controller_write(PS2_CMD_READ_CONFIG);
    uint8_t config = data_read();
    config |= 0x02;
    config &= (uint8_t)~0x20;
    controller_write(PS2_CMD_WRITE_CONFIG);
    data_write(config);

    mouse_write(PS2_MOUSE_SET_DEFAULTS);
    mouse_write(PS2_MOUSE_ENABLE_STREAMING);
    unmask_irq12();

    packet_index = 0;
    serial_write("[mouse] ps2 mouse irq enabled\n");
}

struct interrupt_frame *mouse_handle_irq(struct interrupt_frame *frame)
{
    uint8_t value = inb(PS2_DATA_PORT);

    if (packet_index == 0 && (value & 0x08) == 0) {
        outb(PIC2_COMMAND, PIC_EOI);
        outb(PIC1_COMMAND, PIC_EOI);
        return frame;
    }

    packet[packet_index++] = value;
    if (packet_index == 3) {
        packet_index = 0;

        if ((packet[0] & 0xc0) == 0) {
            int16_t dx = sign_extend_byte(packet[1], (packet[0] & 0x10) != 0);
            int16_t dy = sign_extend_byte(packet[2], (packet[0] & 0x20) != 0);
            input_push_mouse(dx, (int16_t)-dy, packet[0] & 0x07);
        }
    }

    outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
    return frame;
}
