#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void timer_init(uint32_t frequency);
uint32_t timer_ticks(void);
void timer_wait(uint32_t ticks);

#endif
