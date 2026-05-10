#ifndef ZENITH_MOUSE_H
#define ZENITH_MOUSE_H

#include "zenith/interrupt_frame.h"

void mouse_init(void);
struct interrupt_frame *mouse_handle_irq(struct interrupt_frame *frame);

#endif
