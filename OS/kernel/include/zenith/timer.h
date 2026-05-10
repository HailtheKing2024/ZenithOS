#ifndef ZENITH_TIMER_H
#define ZENITH_TIMER_H

#include "zenith/interrupt_frame.h"

#include <stdint.h>

void timer_init(uint32_t frequency_hz);
struct interrupt_frame *timer_handle_irq(struct interrupt_frame *frame);
uint64_t timer_ticks(void);
void timer_wait_ticks(uint64_t ticks);
void timer_self_test(void);

#endif
