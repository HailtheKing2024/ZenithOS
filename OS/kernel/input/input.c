#include "zenith/input.h"

#include "zenith/panic.h"
#include "zenith/serial.h"

#include <stddef.h>

enum {
    INPUT_QUEUE_LENGTH = 32,
    SERIAL_DRAIN_LIMIT = 16
};

static struct input_event queue[INPUT_QUEUE_LENGTH];
static uint64_t head;
static uint64_t tail;
static uint64_t count;

void input_init(void)
{
    head = 0;
    tail = 0;
    count = 0;
    serial_write("[input] event queue initialized\n");
}

void input_push_key(uint64_t code, uint64_t value)
{
    if (count == INPUT_QUEUE_LENGTH) {
        head = (head + 1) % INPUT_QUEUE_LENGTH;
        count--;
    }

    queue[tail] = (struct input_event){
        .type = INPUT_EVENT_KEY,
        .code = code,
        .value = value
    };
    tail = (tail + 1) % INPUT_QUEUE_LENGTH;
    count++;
}

void input_push_mouse(int16_t dx, int16_t dy, uint64_t buttons)
{
    if (count == INPUT_QUEUE_LENGTH) {
        head = (head + 1) % INPUT_QUEUE_LENGTH;
        count--;
    }

    uint64_t packed_delta = ((uint64_t)(uint16_t)dy << 16) | (uint64_t)(uint16_t)dx;
    queue[tail] = (struct input_event){
        .type = INPUT_EVENT_MOUSE,
        .code = buttons,
        .value = packed_delta
    };
    tail = (tail + 1) % INPUT_QUEUE_LENGTH;
    count++;
}

bool input_poll(struct input_event *event_out)
{
    if (event_out == NULL || count == 0) {
        return false;
    }

    *event_out = queue[head];
    head = (head + 1) % INPUT_QUEUE_LENGTH;
    count--;
    return true;
}

static void drain_serial_input(void)
{
    for (uint64_t i = 0; i < SERIAL_DRAIN_LIMIT; i++) {
        char value = 0;
        if (!serial_try_read_char(&value)) {
            return;
        }

        if (value == '\r') {
            value = '\n';
        }

        input_push_key((uint64_t)(uint8_t)value, 1);
    }
}

uint64_t input_poll_packed(void)
{
    struct input_event event;

    drain_serial_input();

    if (!input_poll(&event)) {
        return 0;
    }

    return (event.type << 56) | ((event.code & 0xffffff) << 32) | (event.value & 0xffffffff);
}

void input_self_test(void)
{
    input_push_key('T', 1);

    struct input_event event = {0};
    if (!input_poll(&event) || event.type != INPUT_EVENT_KEY || event.code != 'T') {
        panic("input self-test failed");
    }

    serial_write("[input] self-test ok code=");
    serial_write_u64_dec(event.code);
    serial_write("\n");
}
