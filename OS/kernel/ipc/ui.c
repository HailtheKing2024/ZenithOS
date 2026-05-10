#include "zenith/ui.h"

#include "zenith/panic.h"
#include "zenith/serial.h"

#include <stdbool.h>
#include <stddef.h>

enum {
    UI_QUEUE_LENGTH = 64
};

static struct ui_message queue[UI_QUEUE_LENGTH];
static uint64_t head;
static uint64_t tail;
static uint64_t count;
static uint64_t session_pid;

static void copy_text(char *dest, const char *src)
{
    if (src == NULL) {
        dest[0] = '\0';
        return;
    }

    uint64_t index = 0;
    while (index + 1 < UI_TEXT_CAP && src[index] != '\0') {
        dest[index] = src[index];
        index++;
    }

    dest[index] = '\0';
}

void ui_init(void)
{
    head = 0;
    tail = 0;
    count = 0;
    session_pid = 0;
    serial_write("[ui] session message queue initialized\n");
}

uint64_t ui_bind_session(uint64_t pid)
{
    if (pid == 0) {
        return UI_ERR_INVALID;
    }

    session_pid = pid;
    serial_write("[ui] session bound pid=");
    serial_write_u64_dec(pid);
    serial_write("\n");
    return UI_OK;
}

enum ui_result ui_send(uint64_t source,
                       uint64_t target,
                       uint64_t type,
                       uint64_t a,
                       uint64_t b,
                       uint64_t c,
                       const char *text)
{
    if (source == 0 || type == UI_MSG_NONE) {
        return UI_ERR_INVALID;
    }

    if (target == 0) {
        if (session_pid == 0) {
            return UI_ERR_INVALID;
        }
        target = session_pid;
    }

    if (count == UI_QUEUE_LENGTH) {
        return UI_ERR_FULL;
    }

    queue[tail] = (struct ui_message){
        .type = type,
        .source = source,
        .target = target,
        .a = a,
        .b = b,
        .c = c
    };
    copy_text(queue[tail].text, text);

    tail = (tail + 1) % UI_QUEUE_LENGTH;
    count++;
    return UI_OK;
}

enum ui_result ui_receive(uint64_t target, struct ui_message *message_out)
{
    if (target == 0 || message_out == NULL) {
        return UI_ERR_INVALID;
    }

    for (uint64_t i = 0; i < count; i++) {
        uint64_t index = (head + i) % UI_QUEUE_LENGTH;
        if (queue[index].target == target) {
            *message_out = queue[index];

            while (index != head) {
                uint64_t previous = (index + UI_QUEUE_LENGTH - 1) % UI_QUEUE_LENGTH;
                queue[index] = queue[previous];
                index = previous;
            }

            head = (head + 1) % UI_QUEUE_LENGTH;
            count--;
            return UI_OK;
        }
    }

    return UI_ERR_EMPTY;
}

void ui_self_test(void)
{
    struct ui_message message = {0};

    if (ui_bind_session(3) != UI_OK ||
        ui_send(7, 0, UI_MSG_NOTIFY, 1, 2, 3, "OK") != UI_OK ||
        ui_receive(3, &message) != UI_OK ||
        message.source != 7 ||
        message.type != UI_MSG_NOTIFY ||
        message.a != 1 ||
        message.b != 2 ||
        message.c != 3 ||
        message.text[0] != 'O' ||
        message.text[1] != 'K') {
        panic("ui self-test failed");
    }

    session_pid = 0;
    serial_write("[ui] self-test ok\n");
}
