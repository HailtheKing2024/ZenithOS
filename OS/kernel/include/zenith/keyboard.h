#ifndef ZENITH_KEYBOARD_H
#define ZENITH_KEYBOARD_H

#include "zenith/interrupt_frame.h"

void keyboard_init(void);
struct interrupt_frame *keyboard_handle_irq(struct interrupt_frame *frame);

#endif
