#ifndef TIMER_H
#define TIMER_H

#include "types.h"
#include "isr.h"

void timer_init(uint32_t frequency);
uint32_t timer_ticks(void);
void timer_wait(uint32_t ticks);
void timer_set_scheduler(void (*cb)(struct registers *));

#endif
