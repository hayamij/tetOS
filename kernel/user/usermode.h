#ifndef USERMODE_H
#define USERMODE_H

#include "../types/types.h"

void enter_user_mode(void (*entry)(void), uint32_t user_stack_top);

#endif