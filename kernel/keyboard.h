#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

#define KEY_BUFFER_SIZE 256

void keyboard_init(void);
char keyboard_getchar(void);
int keyboard_has_input(void);

#endif
