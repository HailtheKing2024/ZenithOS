#ifndef ZENITH_UI_H
#define ZENITH_UI_H

#include <stdint.h>

enum {
    UI_TEXT_CAP = 64
};

enum ui_result {
    UI_OK = 0,
    UI_ERR_FULL = 1,
    UI_ERR_EMPTY = 2,
    UI_ERR_INVALID = 3
};

enum ui_message_type {
    UI_MSG_NONE = 0,
    UI_MSG_REGISTER = 1,
    UI_MSG_SET_LINE = 2,
    UI_MSG_CLEAR = 3,
    UI_MSG_INPUT_KEY = 4,
    UI_MSG_INPUT_MOUSE = 5,
    UI_MSG_COMMAND = 6,
    UI_MSG_NOTIFY = 7,
    UI_MSG_CLOSE = 8
};

struct ui_message {
    uint64_t type;
    uint64_t source;
    uint64_t target;
    uint64_t a;
    uint64_t b;
    uint64_t c;
    char text[UI_TEXT_CAP];
};

void ui_init(void);
uint64_t ui_bind_session(uint64_t pid);
enum ui_result ui_send(uint64_t source,
                       uint64_t target,
                       uint64_t type,
                       uint64_t a,
                       uint64_t b,
                       uint64_t c,
                       const char *text);
enum ui_result ui_receive(uint64_t target, struct ui_message *message_out);
void ui_self_test(void);

#endif
