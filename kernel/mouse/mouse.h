#ifndef MOUSE_H
#define MOUSE_H

#include "../types/types.h"

#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04

struct mouse_event {
    int8_t dx;
    int8_t dy;
    uint8_t buttons;
    int32_t x;
    int32_t y;
};

void mouse_init(void);
int mouse_has_event(void);
int mouse_read_event(struct mouse_event *event);
void mouse_get_position(int32_t *x, int32_t *y);
uint8_t mouse_get_buttons(void);

#endif
