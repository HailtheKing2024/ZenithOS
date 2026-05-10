#ifndef ZENITH_INPUT_H
#define ZENITH_INPUT_H

#include <stdbool.h>
#include <stdint.h>

enum input_event_type {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_KEY = 1,
    INPUT_EVENT_MOUSE = 2
};

struct input_event {
    uint64_t type;
    uint64_t code;
    uint64_t value;
};

void input_init(void);
void input_push_key(uint64_t code, uint64_t value);
void input_push_mouse(int16_t dx, int16_t dy, uint64_t buttons);
bool input_poll(struct input_event *event_out);
uint64_t input_poll_packed(void);
void input_self_test(void);

#endif
